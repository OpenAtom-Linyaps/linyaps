/* SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.  + *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "run_context.h"

#include "linglong/common/display.h"
#include "linglong/extension/extension.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/log/log.h"

#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace linglong::runtime {

RuntimeLayer::RuntimeLayer(package::Reference ref, RunContext &context)
    : reference(std::move(ref))
    , runContext(context)
    , temporary(false)
{
}

RuntimeLayer::~RuntimeLayer()
{
    if (temporary && layerDir) {
        layerDir->removeRecursively();
    }
}

utils::error::Result<void> RuntimeLayer::resolveLayer(const std::vector<std::string> &modules,
                                                      const std::optional<std::string> &subRef)
{
    LINGLONG_TRACE("resolve layer");

    auto &repo = runContext.get().getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    if (modules.empty()) {
        layer = repo.getMergedModuleDir(reference);
    } else if (modules.size() > 1) {
        layer = repo.getMergedModuleDir(reference, modules);
        temporary = true;
    } else {
        layer = repo.getLayerDir(reference, modules[0], subRef);
    }

    if (!layer) {
        return LINGLONG_ERR("layer doesn't exist: " + reference.toString(), layer);
    }

    layerDir = *layer;
    return LINGLONG_OK;
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem> RuntimeLayer::getCachedItem()
{
    LINGLONG_TRACE("get cached item");

    if (!cachedItem) {
        auto &repo = runContext.get().getRepo();
        auto item = repo.getLayerItem(reference);
        if (!item) {
            return LINGLONG_ERR("no cached item found: " + reference.toString(), item);
        }
        cachedItem = std::move(item).value();
    }

    return *cachedItem;
}

RunContext::~RunContext()
{
    if (!bundle.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(bundle, ec)) {
            if (std::filesystem::remove_all(bundle, ec) == static_cast<std::uintmax_t>(-1)) {
                LogW("failed to remove bundle directory {}: {}", bundle, ec.message());
            }
        }
    }
}

utils::error::Result<void> RunContext::resolve(const linglong::package::Reference &runnable,
                                               const ResolveOptions &options)
{
    LINGLONG_TRACE("resolve RunContext from runnable " + runnable.toString());

    containerID = runtime::genContainerID(runnable);

    auto item = repo.getLayerItem(runnable);
    if (!item) {
        return LINGLONG_ERR("no cached item found: " + runnable.toString(), item);
    }
    const auto &info = item->info;

    // only base need resolved
    if (info.kind == "base") {
        baseLayer = RuntimeLayer(runnable, *this);
        return resolveExtension(*baseLayer);
    }

    if (info.kind == "app") {
        appLayer = RuntimeLayer(runnable, *this);
        auto runtime = options.runtimeRef.value_or(info.runtime.value_or(""));
        if (!runtime.empty()) {
            auto runtimeFuzzyRef = package::FuzzyReference::parse(runtime);
            if (!runtimeFuzzyRef) {
                return LINGLONG_ERR(runtimeFuzzyRef);
            }

            auto ref = repo.clearReference(*runtimeFuzzyRef,
                                           {
                                             .forceRemote = false,
                                             .fallbackToRemote = false,
                                             .semanticMatching = true,
                                           });
            if (!ref) {
                return LINGLONG_ERR("ref doesn't exist " + runtimeFuzzyRef->toString());
            }
            runtimeLayer = RuntimeLayer(std::move(ref).value(), *this);
        }
    } else if (info.kind == "runtime") {
        runtimeLayer = RuntimeLayer(runnable, *this);
    } else {
        return LINGLONG_ERR("kind " + QString::fromStdString(info.kind) + " is not runnable");
    }

    // all kinds of package has base
    auto baseRef = options.baseRef.value_or(info.base);
    auto baseFuzzyRef = package::FuzzyReference::parse(baseRef);
    if (!baseFuzzyRef) {
        return LINGLONG_ERR(baseFuzzyRef);
    }

    auto ref = repo.clearReference(*baseFuzzyRef,
                                   {
                                     .forceRemote = false,
                                     .fallbackToRemote = false,
                                     .semanticMatching = true,
                                   });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    baseLayer = RuntimeLayer(std::move(ref).value(), *this);

    // resolve runtime extension
    if (runtimeLayer) {
        auto ret = resolveExtension(*runtimeLayer);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // resolve base extension
    auto ret = resolveExtension(*baseLayer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    // 手动解析多个扩展
    if (options.extensionRefs && !options.extensionRefs->empty()) {
        auto manualExtensionDef = makeManualExtensionDefine(*options.extensionRefs);
        if (!manualExtensionDef) {
            return LINGLONG_ERR(manualExtensionDef);
        }

        auto ret = resolveExtension(*manualExtensionDef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // all reference are cleard , we can get actual layer directory now
    return resolveLayer(options.depsBinaryOnly,
                        options.appModules.value_or(std::vector<std::string>{}));
}

utils::error::Result<void> RunContext::resolve(const api::types::v1::BuilderProject &target,
                                               const std::filesystem::path &buildOutput)
{
    LINGLONG_TRACE("resolve RunContext from builder project " + target.package.id);

    auto targetRef = package::Reference::fromBuilderProject(target);
    if (!targetRef) {
        return LINGLONG_ERR(targetRef);
    }
    containerID = runtime::genContainerID(*targetRef);
    targetId = target.package.id;

    if (target.package.kind == "extension") {
        extensionOutput = buildOutput;
    } else if (target.package.kind == "app") {
        appOutput = buildOutput;
    } else if (target.package.kind == "runtime") {
        runtimeOutput = buildOutput;
    } else {
        return LINGLONG_ERR("can't resolve run context from package kind "
                            + QString::fromStdString(target.package.kind));
    }

    auto baseFuzzyRef = package::FuzzyReference::parse(target.base);
    if (!baseFuzzyRef) {
        return LINGLONG_ERR(baseFuzzyRef);
    }

    auto ref = repo.clearReference(*baseFuzzyRef,
                                   {
                                     .forceRemote = false,
                                     .fallbackToRemote = false,
                                     .semanticMatching = true,
                                   });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    baseLayer = RuntimeLayer(std::move(ref).value(), *this);

    if (target.runtime) {
        auto runtimeFuzzyRef = package::FuzzyReference::parse(*target.runtime);
        if (!runtimeFuzzyRef) {
            return LINGLONG_ERR(runtimeFuzzyRef);
        }

        ref = repo.clearReference(*runtimeFuzzyRef,
                                  {
                                    .forceRemote = false,
                                    .fallbackToRemote = false,
                                    .semanticMatching = true,
                                  });
        if (!ref) {
            return LINGLONG_ERR("ref doesn't exist " + runtimeFuzzyRef->toString());
        }
        runtimeLayer = RuntimeLayer(std::move(ref).value(), *this);

        auto layer = runtimeLayer->getCachedItem();
        if (!layer) {
            return LINGLONG_ERR("no cached item found: " + runtimeLayer->getReference().toString(),
                                layer);
        }

        auto fuzzyRef = package::FuzzyReference::parse(layer->info.base);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }
        auto ref = repo.clearReference(*fuzzyRef,
                                       {
                                         .forceRemote = false,
                                         .fallbackToRemote = false,
                                         .semanticMatching = true,
                                       });
        if (!ref || *ref != baseLayer->getReference()) {
            auto msg = fmt::format("Base is not compatible with runtime. \n - Current base: {}\n - "
                                   "Current runtime: {}\n - Base required by runtime: {}",
                                   baseLayer->getReference().toString(),
                                   runtimeLayer->getReference().toString(),
                                   layer->info.base);
            return LINGLONG_ERR(msg);
        }
    }

    return resolveLayer(false, {});
}

utils::error::Result<void> RunContext::resolveLayer(bool depsBinaryOnly,
                                                    const std::vector<std::string> &appModules)
{
    LINGLONG_TRACE("resolve layers");

    std::optional<std::string> subRef;
    if (appLayer) {
        auto item = appLayer->getCachedItem();
        if (item && item->info.uuid) {
            subRef = item->info.uuid;
        }
    }

    std::vector<std::string> depsModules;
    if (depsBinaryOnly) {
        depsModules.emplace_back("binary");
    }
    auto ref = baseLayer->resolveLayer(depsModules, subRef);
    if (!ref.has_value()) {
        return LINGLONG_ERR("failed to resolve base layer", ref);
    }

    if (appLayer) {
        auto ref = appLayer->resolveLayer(appModules);
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to resolve app layer", ref);
        }
    }

    if (runtimeLayer) {
        auto ref = runtimeLayer->resolveLayer(depsModules, subRef);
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to resolve runtime layer", ref);
        }
    }

    for (auto &ext : extensionLayers) {
        if (!ext.resolveLayer()) {
            qWarning() << "ignore failed extension layer";
            continue;
        }

        auto extensionOf = ext.getExtensionInfo();
        if (!extensionOf) {
            continue;
        }

        const auto &[extensionDefine, layer] = *extensionOf;
        auto extItem = ext.getCachedItem();
        if (!extItem) {
            continue;
        }
        const auto &extInfo = extItem->info;
        if (!extInfo.extImpl) {
            LogW("no ext_impl found for {}", ext.getReference().toString());
            continue;
        }
        const auto &extImpl = *extInfo.extImpl;
        if (!extImpl.env) {
            continue;
        }
        for (const auto &env : *extImpl.env) {
            // if allowEnv is not defined, all envs are allowed
            std::string defaultValue;
            if (extensionDefine.allowEnv) {
                const auto &allowEnv = *extensionDefine.allowEnv;
                auto allowed = allowEnv.find(env.first);
                if (allowed == allowEnv.end()) {
                    LogW("env {} not allowed in {}", env.first, ext.getReference().toString());
                    continue;
                }
                defaultValue = allowed->second;
            }

            std::string res =
              common::strings::replaceSubstring(env.second,
                                                "$PREFIX",
                                                "/opt/extensions/" + ext.getReference().id);
            auto &value = environment[env.first];
            if (value.empty()) {
                value = defaultValue;
            }
            // If $ORIGIN is unset and the default value is empty, the environment variable
            // may become ":NEW_VALUE" or "NEW_VALUE:". We cannot remove the leading/trailing
            // colon because the value might represent a non-path element (e.g., a delimiter)
            res = common::strings::replaceSubstring(res, "$ORIGIN", value);

            value = res;
            LogD("environment[{}]={}", env.first, res);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveExtension(RuntimeLayer &layer)
{
    LINGLONG_TRACE("resolve RuntimeLayer extension");

    auto item = layer.getCachedItem();
    if (!item) {
        return LINGLONG_ERR("no cached item found: " + layer.getReference().toString(), item);
    }

    const auto &info = item->info;
    if (info.extensions) {
        return resolveExtension(*info.extensions, info.channel, true);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
RunContext::resolveExtension(const std::vector<api::types::v1::ExtensionDefine> &extDefs,
                             std::optional<std::string> channel,
                             bool skipOnNotFound)
{
    LINGLONG_TRACE("resolve extension define");

    for (const auto &extDef : extDefs) {
        LogD("handle extensions: {}", extDef.name);
        LogD("version: {}", extDef.version);
        LogD("directory: {}", extDef.directory);
        if (extDef.allowEnv) {
            for (const auto &allowEnv : *extDef.allowEnv) {
                LogD("allowEnv: {}:{}", allowEnv.first, allowEnv.second);
            }
        }

        std::string name = extDef.name;
        auto ext = extension::ExtensionFactory::makeExtension(name);
        if (!ext->shouldEnable(name)) {
            continue;
        }

        std::optional<std::string> version;
        if (!extDef.version.empty()) {
            version = extDef.version;
        }
        auto fuzzyRef = package::FuzzyReference::create(channel, name, version, std::nullopt);
        auto ref =
          repo.clearReference(*fuzzyRef, { .fallbackToRemote = false, .semanticMatching = true });
        if (!ref) {
            LogD("extension is not installed: {}", fuzzyRef->toString());
            if (extension::isNvidiaDisplayDriverExtension(name)) {
                hostExtensions.insert(name);
                continue;
            }
            if (skipOnNotFound) {
                continue;
            }
            return LINGLONG_ERR("extension is not installed", ref);
        }

        RuntimeLayer layer(*ref, *this);
        auto item = layer.getCachedItem();
        if (!item) {
            return LINGLONG_ERR("failed to get layer item", item);
        }
        if (item->info.kind != "extension") {
            return LINGLONG_ERR(fmt::format("{} is not an extension", ref->toString()));
        }

        auto &extensionLayer = extensionLayers.emplace_back(std::move(layer));
        extensionLayer.setExtensionInfo(
          std::make_pair(extDef, std::reference_wrapper<RuntimeLayer>(extensionLayer)));
    }

    return LINGLONG_OK;
}

utils::error::Result<std::vector<api::types::v1::ExtensionDefine>>
RunContext::makeManualExtensionDefine(const std::vector<std::string> &refs)
{
    LINGLONG_TRACE("make extension define");

    std::vector<api::types::v1::ExtensionDefine> extDefs;
    extDefs.reserve(refs.size());
    for (const auto &ref : refs) {
        auto fuzzyRef = package::FuzzyReference::parse(ref);
        if (!fuzzyRef) {
            return LINGLONG_ERR("failed to parse extension ref", fuzzyRef);
        }

        extDefs.emplace_back(api::types::v1::ExtensionDefine{
          .directory = "/opt/extensions/" + fuzzyRef->id,
          .name = fuzzyRef->id,
          .version = fuzzyRef->version.value_or(""),
        });
    }
    return extDefs;
}

void RunContext::detectDisplaySystem(generator::ContainerCfgBuilder &builder) noexcept
{
    while (true) {
        auto *xOrgDisplayEnv = ::getenv("DISPLAY");
        if (xOrgDisplayEnv == nullptr || xOrgDisplayEnv[0] == '\0') {
            LogD("DISPLAY is not set, ignore it");
            break;
        }

        auto xOrgDisplay = common::display::getXOrgDisplay(xOrgDisplayEnv);
        if (!xOrgDisplay) {
            LogW("failed to get XOrg display: {}, ignore it", xOrgDisplay.error());
            break;
        }

        builder.bindXOrgSocket(xOrgDisplay.value());
        break;
    }

    while (true) {
        auto *xOrgAuthFileEnv = ::getenv("XAUTHORITY");
        if (xOrgAuthFileEnv == nullptr || xOrgAuthFileEnv[0] == '\0') {
            LogD("XAUTHORITY is not set, ignore it");
            break;
        }

        auto xOrgAuthFile = common::display::getXOrgAuthFile(xOrgAuthFileEnv);
        if (!xOrgAuthFile) {
            LogW("failed to get XOrg auth file: {}, ignore it", xOrgAuthFile.error());
            break;
        }

        builder.bindXAuthFile(xOrgAuthFile.value());
        break;
    }

    while (true) {
        auto *waylandDisplayEnv = ::getenv("WAYLAND_DISPLAY");
        if (waylandDisplayEnv == nullptr || waylandDisplayEnv[0] == '\0') {
            LogD("WAYLAND_DISPLAY is not set, ignore it");
            break;
        }

        auto waylandDisplay = common::display::getWaylandDisplay(waylandDisplayEnv);
        if (!waylandDisplay) {
            LogW("failed to get Wayland display: {}, ignore it", waylandDisplay.error());
            break;
        }

        builder.bindWaylandSocket(waylandDisplay.value());
        break;
    }
}

void RunContext::applyExtensionOverrides(
  generator::ContainerCfgBuilder &builder,
  std::vector<ocppi::runtime::config::types::Mount> &extensionMounts)
{
    if (extensionOverrides.empty()) {
        return;
    }

    auto matchesPrefix = [](const std::string &extensionName, const std::string &prefix) -> bool {
        if (extensionName == prefix) {
            return true;
        }
        if (extensionName.size() <= prefix.size()) {
            return false;
        }
        return extensionName.compare(0, prefix.size(), prefix) == 0
          && extensionName[prefix.size()] == '.';
    };
    auto hasInstalledExtension = [&](const std::string &prefix) -> bool {
        for (const auto &ext : extensionLayers) {
            if (matchesPrefix(ext.getReference().id, prefix)) {
                return true;
            }
        }
        return false;
    };
    auto ensureExtensionRoot = [&](const std::string &rootName, bool has32Bit) {
        if (rootName.empty()) {
            return;
        }
        std::string destination = "/opt/extensions/" + rootName;
        for (const auto &mount : extensionMounts) {
            if (mount.destination == destination) {
                return;
            }
        }
        if (bundle.empty()) {
            LogW("bundle path is empty, skip CDI extension mount for {}", rootName);
            return;
        }
        std::filesystem::path sourceDir = bundle / "cdi-extensions" / rootName;
        std::error_code ec;
        std::filesystem::create_directories(sourceDir / "etc", ec);
        if (ec) {
            LogW("failed to create CDI extension dir {}: {}", sourceDir.string(), ec.message());
            return;
        }
        std::filesystem::create_directories(sourceDir / "orig", ec);
        if (has32Bit) {
            std::filesystem::create_directories(sourceDir / "orig/32", ec);
        }

        std::ofstream ldConf(sourceDir / "etc/ld.so.conf", std::ios::binary | std::ios::out | std::ios::trunc);
        if (ldConf.is_open()) {
            ldConf << destination << "/orig\n";
            if (has32Bit) {
                ldConf << destination << "/orig/32\n";
            }
        }

        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = destination,
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = sourceDir.string(),
          .type = "bind",
          .uidMappings = {},
        });
    };
    auto collectExtensionRoots = [&](const ExtensionOverride &overrideDef) {
        std::unordered_map<std::string, bool> roots;
        const std::string prefix = "/opt/extensions/";
        for (const auto &mount : overrideDef.mounts) {
            if (mount.destination.empty()) {
                continue;
            }
            const auto &dest = mount.destination;
            if (dest.rfind(prefix, 0) != 0) {
                continue;
            }
            auto rest = dest.substr(prefix.size());
            if (rest.empty()) {
                continue;
            }
            auto slash = rest.find('/');
            std::string root = (slash == std::string::npos) ? rest : rest.substr(0, slash);
            if (root.empty()) {
                continue;
            }
            bool has32 = dest.find("/orig/32") != std::string::npos;
            auto it = roots.find(root);
            if (it == roots.end()) {
                roots.emplace(root, has32);
            } else if (has32) {
                it->second = true;
            }
        }
        for (const auto &item : roots) {
            ensureExtensionRoot(item.first, item.second);
        }
    };

    std::unordered_map<std::string, std::filesystem::path> hostDirRelMap;
    std::unordered_map<std::string, int> hostDirCounters;
    std::unordered_set<std::string> hostDirMounts;
    auto ensureHostDir = [&](const std::string &rootName,
                             const std::filesystem::path &sourceDir,
                             const std::vector<std::string> &options)
      -> std::optional<std::filesystem::path> {
        if (sourceDir.empty() || !sourceDir.is_absolute()) {
            return std::nullopt;
        }
        auto key = rootName + "|" + sourceDir.lexically_normal().string();
        auto it = hostDirRelMap.find(key);
        if (it != hostDirRelMap.end()) {
            return it->second;
        }
        int &counter = hostDirCounters[rootName];
        auto rel = std::filesystem::path("host") / std::to_string(counter++);
        hostDirRelMap.emplace(key, rel);
        std::error_code ec;
        std::filesystem::create_directories(bundle / "cdi-extensions" / rootName / rel, ec);
        ec.clear();
        auto mountKey = rootName + "|" + rel.string();
        if (hostDirMounts.insert(mountKey).second) {
            builder.addExtraMount(ocppi::runtime::config::types::Mount{
              .destination = (std::filesystem::path("/opt/extensions") / rootName / rel).string(),
              .gidMappings = {},
              .options = options,
              .source = sourceDir.string(),
              .type = "bind",
              .uidMappings = {},
            });
        }
        return rel;
    };

    auto linkOverrideMount = [&](const ocppi::runtime::config::types::Mount &mount) -> bool {
        if (!mount.source || mount.source->empty() || mount.destination.empty()) {
            return false;
        }
        std::filesystem::path sourcePath{ *mount.source };
        if (!sourcePath.is_absolute()) {
            return false;
        }
        std::filesystem::path destPath{ mount.destination };
        const std::string prefix = "/opt/extensions/";
        auto destStr = destPath.generic_string();
        if (destStr.rfind(prefix, 0) != 0) {
            return false;
        }
        auto rest = destStr.substr(prefix.size());
        auto slash = rest.find('/');
        if (slash == std::string::npos) {
            return false;
        }
        std::string rootName = rest.substr(0, slash);
        if (rootName.empty()) {
            return false;
        }
        if (bundle.empty()) {
            return false;
        }
        std::filesystem::path rel =
          destPath.lexically_relative(std::filesystem::path("/opt/extensions") / rootName);
        if (rel.empty() || rel == "." || rel.native().rfind("..", 0) == 0) {
            return false;
        }
        std::vector<std::string> options =
          mount.options.value_or(std::vector<std::string>{ "rbind", "ro" });
        auto relDir = ensureHostDir(rootName, sourcePath.parent_path(), options);
        if (!relDir) {
            return false;
        }
        std::filesystem::path destInRoot = bundle / "cdi-extensions" / rootName / rel;
        std::filesystem::path target =
          std::filesystem::path("/opt/extensions") / rootName / *relDir / sourcePath.filename();
        std::error_code ec;
        std::filesystem::create_directories(destInRoot.parent_path(), ec);
        ec.clear();
        if (std::filesystem::is_symlink(destInRoot, ec)) {
            auto existing = std::filesystem::read_symlink(destInRoot, ec);
            if (!ec && existing == target) {
                ec.clear();
                return true;
            }
        }
        ec.clear();
        if (std::filesystem::exists(destInRoot, ec)) {
            std::filesystem::remove(destInRoot, ec);
        }
        ec.clear();
        std::filesystem::create_symlink(target, destInRoot, ec);
        if (ec) {
            LogW("failed to create CDI link {} -> {}: {}",
                 destInRoot.string(),
                 target.string(),
                 ec.message());
            ec.clear();
            return false;
        }
        auto destFilename = destPath.filename();
        auto sourceFilename = sourcePath.filename();
        if (!sourceFilename.empty() && destFilename != sourceFilename) {
            std::filesystem::path aliasPath = destInRoot.parent_path() / sourceFilename;
            std::filesystem::path aliasTarget = destFilename;
            std::error_code aliasEc;
            bool needsCreate = true;
            if (std::filesystem::is_symlink(aliasPath, aliasEc)) {
                auto existing = std::filesystem::read_symlink(aliasPath, aliasEc);
                if (!aliasEc && existing == aliasTarget) {
                    needsCreate = false;
                }
            }
            aliasEc.clear();
            if (needsCreate) {
                if (std::filesystem::exists(aliasPath, aliasEc)) {
                    std::filesystem::remove(aliasPath, aliasEc);
                }
                aliasEc.clear();
                std::filesystem::create_symlink(aliasTarget, aliasPath, aliasEc);
                if (aliasEc) {
                    LogW("failed to create CDI alias {} -> {}: {}",
                         aliasPath.string(),
                         aliasTarget.string(),
                         aliasEc.message());
                }
            }
        }

        return true;
    };

    for (const auto &overrideDef : extensionOverrides) {
        bool installed = hasInstalledExtension(overrideDef.name);
        bool isNvidiaOverride = extension::isNvidiaDisplayDriverExtension(overrideDef.name);
        if (installed && (overrideDef.fallbackOnly || isNvidiaOverride)) {
            continue;
        }

        collectExtensionRoots(overrideDef);

        for (const auto &env : overrideDef.env) {
            environment[env.first] = env.second;
        }

        for (auto mount : overrideDef.mounts) {
            if (mount.destination.empty() || !mount.source) {
                continue;
            }
            if (!mount.options || mount.options->empty()) {
                mount.options = std::vector<std::string>{ "rbind", "ro" };
            }
            if (!mount.type) {
                mount.type = "bind";
            }
            if (linkOverrideMount(mount)) {
                continue;
            }
            builder.addExtraMount(mount);
        }

        for (const auto &node : overrideDef.deviceNodes) {
            ocppi::runtime::config::types::Mount mount = {
                .destination = node.path,
                .options = { { "bind" } },
                .source = node.hostPath.value_or(node.path),
                .type = "bind",
            };
            builder.addExtraMount(mount);
        }
    }
}

utils::error::Result<void>
RunContext::fillContextCfg(linglong::generator::ContainerCfgBuilder &builder,
                           const std::string &bundleSuffix)
{
    LINGLONG_TRACE("fill ContainerCfgBuilder with run context");

    builder.setContainerId(containerID);

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    auto bundleDir = runtime::makeBundleDir(containerID, bundleSuffix);
    if (!bundleDir) {
        return LINGLONG_ERR("failed to get bundle dir of " + QString::fromStdString(containerID));
    }
    bundle = *bundleDir;
    builder.setBundlePath(bundle);

    builder.setBasePath(baseLayer->getLayerDir()->absoluteFilePath("files").toStdString());

    if (appOutput) {
        builder.setAppPath(*appOutput, false);
    } else {
        if (appLayer) {
            builder.setAppPath(appLayer->getLayerDir()->absoluteFilePath("files").toStdString());
        }
    }

    if (runtimeOutput) {
        builder.setRuntimePath(*runtimeOutput, false);
    } else {
        if (runtimeLayer) {
            builder.setRuntimePath(
              runtimeLayer->getLayerDir()->absoluteFilePath("files").toStdString());
        }
    }

    std::vector<ocppi::runtime::config::types::Mount> extensionMounts{};
    if (extensionOutput) {
        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt/extensions/" + targetId,
          .gidMappings = {},
          .options = { { "rbind" } },
          .source = extensionOutput,
          .type = "bind",
          .uidMappings = {},
        });
    }

    for (auto &ext : extensionLayers) {
        auto item = ext.getCachedItem();
        if (!item) {
            continue;
        }
        const auto &info = item->info;
        if (info.extImpl && info.extImpl->deviceNodes) {
            for (auto &node : *info.extImpl->deviceNodes) {
                ocppi::runtime::config::types::Mount mount = {
                    .destination = node.path,
                    .options = { { "bind" } },
                    .source = node.hostPath.value_or(node.path),
                    .type = "bind",
                };
                builder.addExtraMount(mount);
            }
        }

        std::string name = ext.getReference().id;
        if (extensionOutput && name == targetId) {
            continue;
        }
        extensionMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt/extensions/" + name,
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = ext.getLayerDir()->absoluteFilePath("files").toStdString(),
          .type = "bind",
          .uidMappings = {},
        });
    }

    setupHostNvidiaFallbacks(builder, extensionMounts);
    applyExtensionOverrides(builder, extensionMounts);
    if (!extensionMounts.empty()) {
        builder.setExtensionMounts(extensionMounts);
    }

    auto res = fillExtraAppMounts(builder);
    if (!res) {
        return res;
    }

    if (!environment.empty()) {
        builder.appendEnv(environment);
    }

    detectDisplaySystem(builder);

    for (auto ctx = securityContexts.begin(); ctx != securityContexts.end(); ++ctx) {
        auto manager = getSecurityContextManager(ctx->first);
        if (!manager) {
            auto msg = "failed to get security context manager: " + fromType(ctx->first);
            return LINGLONG_ERR(msg.c_str());
        }

        auto secCtx = manager->createSecurityContext(builder);
        if (!secCtx) {
            auto msg = "failed to create security context: " + fromType(ctx->first);
            return LINGLONG_ERR(msg.c_str());
        }
        ctx->second = std::move(secCtx).value();

        auto res = ctx->second->apply(builder);
        if (!res) {
            auto msg = "failed to apply security context: " + fromType(ctx->first);
            ctx = securityContexts.erase(ctx);
            return LINGLONG_ERR(msg.c_str(), res);
        }
    }

    return LINGLONG_OK;
}

void RunContext::enableSecurityContext(const std::vector<SecurityContextType> &ctxs)
{
    for (const auto &type : ctxs) {
        securityContexts.try_emplace(type, nullptr);
    }
}

utils::error::Result<void> RunContext::fillExtraAppMounts(generator::ContainerCfgBuilder &builder)
{
    LINGLONG_TRACE("fill extra app mounts");

    auto fillPermissionsBinds = [&builder,
                                 this](RuntimeLayer &layer) -> utils::error::Result<void> {
        LINGLONG_TRACE("fill permissions binds");

        auto item = layer.getCachedItem();
        if (!item) {
            return LINGLONG_ERR(item);
        }
        const auto &info = item->info;

        if (info.permissions) {
            std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
            auto bindMount =
              [&applicationMounts](
                const api::types::v1::ApplicationConfigurationPermissionsBind &bind) {
                  applicationMounts.push_back(ocppi::runtime::config::types::Mount{
                    .destination = bind.destination,
                    .gidMappings = {},
                    .options = { { "rbind" } },
                    .source = bind.source,
                    .type = "bind",
                    .uidMappings = {},
                  });
              };

            auto bindInnerMount =
              [&applicationMounts,
               this](const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
                  applicationMounts.push_back(ocppi::runtime::config::types::Mount{
                    .destination = bind.destination,
                    .gidMappings = {},
                    .options = { { "rbind" } },
                    .source = bundle.string() + "/rootfs" + bind.source,
                    .type = "bind",
                    .uidMappings = {},
                  });
              };

            const auto &perm = info.permissions;
            if (perm->binds) {
                const auto &binds = perm->binds;
                std::for_each(binds->cbegin(), binds->cend(), bindMount);
            }

            if (perm->innerBinds) {
                const auto &innerBinds = perm->innerBinds;
                std::for_each(innerBinds->cbegin(), innerBinds->cend(), bindInnerMount);
            }

            builder.addExtraMounts(applicationMounts);
        }

        return LINGLONG_OK;
    };

    if (appLayer) {
        auto res = fillPermissionsBinds(*appLayer);
        if (!res) {
            return LINGLONG_ERR("failed to apply permission binds for "
                                  + appLayer->getReference().toString(),
                                res);
        }
    }

    for (auto &ext : extensionLayers) {
        if (!fillPermissionsBinds(ext)) {
            LogW("failed to apply permission binds for {}", ext.getReference().toString());
            continue;
        }
    }

    return LINGLONG_OK;
}

api::types::v1::ContainerProcessStateInfo RunContext::stateInfo()
{
    auto state = linglong::api::types::v1::ContainerProcessStateInfo{
        .containerID = containerID,
    };

    if (baseLayer) {
        state.base = baseLayer->getReference().toString();
    }

    if (appLayer) {
        state.app = appLayer->getReference().toString();
    }

    if (runtimeLayer) {
        state.runtime = runtimeLayer->getReference().toString();
    }

    state.extensions = std::vector<std::string>{};
    for (auto &ext : extensionLayers) {
        state.extensions->push_back(ext.getReference().toString());
    }
    for (const auto &name : hostExtensions) {
        state.extensions->push_back(name);
    }

    return state;
}

utils::error::Result<std::filesystem::path> RunContext::getBaseLayerPath() const
{
    LINGLONG_TRACE("get base layer path");

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    const auto &layerDir = baseLayer->getLayerDir();
    return std::filesystem::path{ layerDir->absolutePath().toStdString() };
}

utils::error::Result<std::filesystem::path> RunContext::getRuntimeLayerPath() const
{
    LINGLONG_TRACE("get runtime layer path");

    if (!runtimeLayer) {
        return LINGLONG_ERR("no runtime layer exist");
    }

    const auto &layerDir = runtimeLayer->getLayerDir();
    return std::filesystem::path{ layerDir->absolutePath().toStdString() };
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem> RunContext::getCachedAppItem()
{
    LINGLONG_TRACE("get cached app item");

    if (!appLayer) {
        return LINGLONG_ERR("no app layer exist");
    }

    return appLayer->getCachedItem();
}

} // namespace linglong::runtime
