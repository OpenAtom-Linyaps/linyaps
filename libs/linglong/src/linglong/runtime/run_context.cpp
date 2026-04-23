/* SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.  + *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "run_context.h"

#include "linglong/cdi/cdi.h"
#include "linglong/common/display.h"
#include "linglong/common/strings.h"
#include "linglong/extension/extension.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/overlayfs_driver.h"
#include "linglong/utils/log/log.h"

#include <fmt/ranges.h>

#include <cstdlib>
#include <utility>

namespace linglong::runtime {

namespace {

constexpr const char *runContextConfigVersion = "1";

std::optional<std::string> timezoneFromPath(const std::filesystem::path &path,
                                            const std::filesystem::path &zoneinfoRoot)
{
    auto normalizedPath = path.lexically_normal();
    auto normalizedRoot = zoneinfoRoot.lexically_normal();
    auto relative = normalizedPath.lexically_relative(normalizedRoot);
    if (relative.empty() || relative == "."
        || linglong::common::strings::starts_with(relative.string(), "..")) {
        return std::nullopt;
    }

    return relative.string();
}

} // namespace

utils::error::Result<RuntimeLayer> RuntimeLayer::create(package::Reference ref, RunContext &context)
{
    LINGLONG_TRACE(fmt::format("create runtime layer from ref {}", ref.toString()));

    try {
        return RuntimeLayer(ref, context);
    } catch (const std::exception &e) {
        return LINGLONG_ERR("failed to create runtime layer", e);
    }
}

RuntimeLayer::RuntimeLayer(package::Reference ref, RunContext &context)
    : reference(std::move(ref))
    , runContext(context)
    , temporary(false)
{
    const auto &repo = context.getRepo();
    auto item = repo.getLayerItem(reference);
    if (!item) {
        throw std::runtime_error("no cached item found");
    }
    cachedItem = std::move(item).value();
}

RuntimeLayer::~RuntimeLayer()
{
    if (temporary && layerDir) {
        std::error_code ec;
        std::filesystem::remove_all(layerDir->path(), ec);
    }
}

utils::error::Result<void> RuntimeLayer::resolveLayer(const std::vector<std::string> &modules,
                                                      const std::optional<std::string> &subRef)
{
    LINGLONG_TRACE("resolve layer");

    auto &repo = runContext.get().getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    if (modules.empty() || (modules.size() == 1 && modules[0] == "binary")) {
        layer = repo.getMergedModuleDir(reference, true, subRef);
    } else if (modules.size() > 1) {
        layer = repo.createTempMergedModuleDir(reference, modules);
        temporary = true;
        LogD("create temp merged module dir: {}", layer->path());
    } else {
        return LINGLONG_ERR(
          fmt::format("resolve module {} is not supported", fmt::join(modules, ",")));
    }

    if (!layer) {
        return LINGLONG_ERR("layer doesn't exist: " + reference.toString(), layer);
    }

    layerDir = *layer;
    return LINGLONG_OK;
}

RunContext::~RunContext() { }

utils::error::Result<void> RunContext::resolve(const linglong::package::Reference &runnable,
                                               const ResolveOptions &options)
{
    LINGLONG_TRACE("resolve RunContext from runnable " + runnable.toString());

    auto layer = RuntimeLayer::create(runnable, *this);
    if (!layer) {
        return LINGLONG_ERR(layer);
    }

    auto info = layer->getCachedItem().info;
    targetId = info.id;
    if (info.kind == "base") {
        baseLayer = std::move(layer).value();
    } else if (info.kind == "app") {
        appLayer = std::move(layer).value();
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
            auto res = RuntimeLayer::create(std::move(ref).value(), *this);
            if (!res) {
                return LINGLONG_ERR(res);
            }
            runtimeLayer = std::move(res).value();
        }
    } else if (info.kind == "runtime") {
        runtimeLayer = std::move(layer).value();
    } else {
        return LINGLONG_ERR(fmt::format("kind {} is not runnable", info.kind));
    }

    // base layer must be resolved for all kinds
    if (!baseLayer) {
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
        auto res = RuntimeLayer::create(std::move(ref).value(), *this);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        baseLayer = std::move(res).value();
    }

    // resolve base extension
    auto ret = resolveLayerExtensions(
      *baseLayer,
      matchedExtensionDefines(baseLayer->getReference(), options.externalExtensionDefs));
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    // resolve runtime extension
    if (runtimeLayer) {
        auto ret = resolveLayerExtensions(
          *runtimeLayer,
          matchedExtensionDefines(runtimeLayer->getReference(), options.externalExtensionDefs));
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // resolve app extension
    if (appLayer) {
        auto ret = resolveLayerExtensions(
          *appLayer,
          matchedExtensionDefines(appLayer->getReference(), options.externalExtensionDefs));
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // 手动解析多个扩展
    if (options.extensionRefs && !options.extensionRefs->empty()) {
        auto manualExtensionDef = makeManualExtensionDefine(*options.extensionRefs);
        if (!manualExtensionDef) {
            return LINGLONG_ERR(manualExtensionDef);
        }

        RuntimeLayer *targetLayer = nullptr;
        if (appLayer) {
            targetLayer = &*appLayer;
        } else if (runtimeLayer) {
            targetLayer = &*runtimeLayer;
        } else {
            targetLayer = &*baseLayer;
        }

        auto ret = resolveExtension(*targetLayer, *manualExtensionDef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    auto overlayRet = resolveOverlayMode();
    if (!overlayRet) {
        return LINGLONG_ERR("failed to resolve overlayfs mode", overlayRet);
    }

    auto timezoneRet = resolveTimeZone();
    if (!timezoneRet) {
        return LINGLONG_ERR("failed to resolve timezone", timezoneRet);
    }

    if (options.cdiDevices) {
        contextCfg.cdiDevices = options.cdiDevices.value();
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
    targetId = target.package.id;

    if (target.package.kind == "extension") {
        extensionOutput = buildOutput;
    } else if (target.package.kind == "app") {
        appOutput = buildOutput;
    } else if (target.package.kind == "runtime") {
        runtimeOutput = buildOutput;
    } else {
        return LINGLONG_ERR("can't resolve run context from package kind " + target.package.kind);
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
    auto res = RuntimeLayer::create(std::move(ref).value(), *this);
    if (!res) {
        return LINGLONG_ERR(res);
    }
    baseLayer = std::move(res).value();

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
        auto res = RuntimeLayer::create(std::move(ref).value(), *this);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        runtimeLayer = std::move(res).value();

        const auto &info = runtimeLayer->getCachedItem().info;
        auto fuzzyRef = package::FuzzyReference::parse(info.base);
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
                                   info.base);
            return LINGLONG_ERR(msg);
        }
    }

    auto timezoneRet = resolveTimeZone();
    if (!timezoneRet) {
        return LINGLONG_ERR("failed to resolve timezone", timezoneRet);
    }

    return resolveLayer(false, {});
}

utils::error::Result<void> RunContext::resolve(const api::types::v1::RunContextConfig &config)
{
    LINGLONG_TRACE("resolve RunContext from config");

    if (config.version != runContextConfigVersion) {
        return LINGLONG_ERR(fmt::format("run context config version mismatch: config version {}, "
                                        "expected version {}",
                                        config.version,
                                        runContextConfigVersion));
    }

    auto createLayer = [this](const std::string &refStr) -> utils::error::Result<RuntimeLayer> {
        LINGLONG_TRACE("create runtime layer");
        if (refStr.empty()) {
            return LINGLONG_ERR("reference is empty");
        }

        auto ref = package::Reference::parse(refStr);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }

        auto layer = RuntimeLayer::create(*ref, *this);
        if (!layer) {
            return LINGLONG_ERR(layer);
        }

        return std::move(layer).value();
    };
    auto findTargetLayer = [this, &_linglong_trace_message](const std::string &targetRefStr)
      -> utils::error::Result<std::reference_wrapper<RuntimeLayer>> {
        auto fuzzyRef = package::FuzzyReference::parse(targetRefStr);
        if (!fuzzyRef) {
            return LINGLONG_ERR("failed to parse target layer reference", fuzzyRef);
        }

        auto matches = [&fuzzyRef](RuntimeLayer &layer) {
            const auto &ref = layer.getReference();
            if (fuzzyRef->id != ref.id) {
                return false;
            }
            if (fuzzyRef->channel && *fuzzyRef->channel != ref.channel) {
                return false;
            }
            if (fuzzyRef->version && !ref.version.semanticMatch(*fuzzyRef->version)) {
                return false;
            }
            if (fuzzyRef->arch && *fuzzyRef->arch != ref.arch) {
                return false;
            }

            return true;
        };

        if (appLayer && matches(*appLayer)) {
            return std::ref(*appLayer);
        }
        if (runtimeLayer && matches(*runtimeLayer)) {
            return std::ref(*runtimeLayer);
        }
        if (baseLayer && matches(*baseLayer)) {
            return std::ref(*baseLayer);
        }

        return LINGLONG_ERR(fmt::format("target layer not found: {}", targetRefStr));
    };

    if (config.base && !config.base->empty()) {
        auto result = createLayer(*config.base);
        if (!result) {
            return LINGLONG_ERR("failed to create base layer", result);
        }

        baseLayer = std::move(result).value();
    }

    if (config.runtime && !config.runtime->empty()) {
        auto result = createLayer(*config.runtime);
        if (!result) {
            return LINGLONG_ERR("failed to create runtime layer", result);
        }

        runtimeLayer = std::move(result).value();
    }

    if (config.app && !config.app->empty()) {
        auto result = createLayer(*config.app);
        if (!result) {
            return LINGLONG_ERR("failed to create app layer", result);
        }

        appLayer = std::move(result).value();
    }

    if (appLayer) {
        targetId = appLayer->getReference().id;
    } else if (runtimeLayer) {
        targetId = runtimeLayer->getReference().id;
    } else if (baseLayer) {
        targetId = baseLayer->getReference().id;
    }

    if (!baseLayer) {
        return LINGLONG_ERR("base layer is required");
    }

    if (config.extensions && !config.extensions->empty()) {
        for (const auto &[targetRefStr, extensionRefs] : *config.extensions) {
            auto targetLayer = findTargetLayer(targetRefStr);
            if (!targetLayer) {
                return LINGLONG_ERR(targetLayer);
            }

            for (const auto &extensionRefStr : extensionRefs) {
                auto result = createLayer(extensionRefStr);
                if (!result) {
                    return LINGLONG_ERR("failed to create extension layer", result);
                }

                auto &extLayer = extensionLayers.emplace_back(std::move(result).value());
                if (extLayer.getCachedItem().info.kind != "extension") {
                    return LINGLONG_ERR("invalid extension kind in config.extensions");
                }
                extLayer.setExtensionInfo(RuntimeLayer::ExtensionRuntimeLayerInfo{
                  .extensionInfo =
                    api::types::v1::ExtensionDefine{
                      .directory = "/opt/extensions/" + extLayer.getReference().id,
                      .name = extLayer.getReference().id,
                      .version = extLayer.getReference().version.toString(),
                    },
                  .extensionLayer = std::ref(extLayer),
                  .forRef = targetRefStr,
                });
            }
        }
    }

    if (config.cdiDevices) {
        contextCfg.cdiDevices = config.cdiDevices.value();
    }

    contextCfg.overlayfs = config.overlayfs;
    contextCfg.timezone = config.timezone;
    contextCfg.version = runContextConfigVersion;

    return resolveLayer(false, {});
}

utils::error::Result<void> RunContext::setupCDIDevices(generator::ContainerCfgBuilder &builder,
                                                       bool applyHooks) const
{
    LINGLONG_TRACE("setup CDI devices");

    if (!contextCfg.cdiDevices) {
        return LINGLONG_OK;
    }

    for (const auto &device : *contextCfg.cdiDevices) {
        auto edits = cdi::getCDIDeviceEdits(device);
        if (!edits) {
            return LINGLONG_ERR(
              fmt::format("failed to resolve CDI device edits {}={}", device.kind, device.name),
              edits);
        }

        auto containerEdits = *edits;
        if (!applyHooks) {
            containerEdits.hooks = std::nullopt;
        }

        auto applyRes = builder.applyCDIPatch(containerEdits);
        if (!applyRes) {
            return LINGLONG_ERR("apply CDI device edits", applyRes);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveLayer(bool depsBinaryOnly,
                                                    const std::vector<std::string> &appModules)
{
    LINGLONG_TRACE("resolve layers");

    std::optional<std::string> subRef;
    if (appLayer) {
        const auto &info = appLayer->getCachedItem().info;
        if (info.uuid) {
            subRef = info.uuid;
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
            LogW("ignore failed extension layer");
            continue;
        }

        const auto &extensionOf = ext.getExtensionInfo();
        if (!extensionOf) {
            continue;
        }

        const auto &extensionDefine = extensionOf->extensionInfo;
        const auto &extInfo = ext.getCachedItem().info;
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

    if (baseLayer) {
        contextCfg.base = baseLayer->getReference().toString();
    }

    if (runtimeLayer) {
        contextCfg.runtime = runtimeLayer->getReference().toString();
    }

    if (appLayer) {
        contextCfg.app = appLayer->getReference().toString();
    }

    contextCfg.version = runContextConfigVersion;
    contextCfg.extensions = std::map<std::string, std::vector<std::string>>{};
    for (const auto &extension : extensionLayers) {
        const auto &extInfo = extension.getExtensionInfo();
        if (extInfo) {
            auto forRef = extInfo->forRef;
            auto extRef = extension.getReference().toString();

            if (contextCfg.extensions->find(forRef) == contextCfg.extensions->end()) {
                (*contextCfg.extensions)[forRef] = std::vector<std::string>{};
            }
            (*contextCfg.extensions)[forRef].push_back(extRef);
        }
    }

    containerID = runtime::genContainerID(contextCfg);

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveOverlayMode(std::optional<std::string> requestedMode)
{
    LINGLONG_TRACE("resolve overlayfs mode");

    utils::OverlayMode mode = utils::OverlayMode::Auto;
    if (requestedMode && !requestedMode->empty()) {
        auto parsedMode = OverlayFSDriver::modeFromString(*requestedMode);
        if (!parsedMode) {
            return LINGLONG_ERR(parsedMode);
        }
        mode = *parsedMode;
    }

    auto driver = OverlayFSDriver::create(mode);
    contextCfg.overlayfs = std::string(OverlayFSDriver::modeToString(driver->mode()));

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveTimeZone()
{
    LINGLONG_TRACE("resolve timezone");

    auto *tzdirEnv = std::getenv("TZDIR");
    auto zoneinfoRoot = (tzdirEnv != nullptr && tzdirEnv[0] != '\0')
      ? std::filesystem::path(tzdirEnv)
      : std::filesystem::path("/usr/share/zoneinfo");

    auto localtimePath = std::filesystem::path("/etc/localtime");
    std::error_code ec;
    auto localtimeStatus = std::filesystem::symlink_status(localtimePath, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to get status of {}", localtimePath), ec);
    }

    if (!std::filesystem::exists(localtimeStatus)) {
        contextCfg.timezone = "UTC";
        return LINGLONG_OK;
    }

    if (!std::filesystem::is_symlink(localtimeStatus)) {
        contextCfg.timezone = "";
        return LINGLONG_OK;
    }

    auto targetPath = std::filesystem::read_symlink(localtimePath, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to read symlink {}", localtimePath), ec);
    }
    targetPath = std::filesystem::absolute(localtimePath.parent_path() / targetPath, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to get absolute path of {}", localtimePath), ec);
    }

    auto timezone = timezoneFromPath(targetPath, zoneinfoRoot);
    if (!timezone) {
        auto canonicalPath = std::filesystem::weakly_canonical(targetPath, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to canonicalize {}", targetPath), ec);
        }
        timezone = timezoneFromPath(canonicalPath, zoneinfoRoot);
    }

    if (timezone) {
        contextCfg.timezone = std::move(*timezone);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveLayerExtensions(
  RuntimeLayer &layer, const std::vector<api::types::v1::ExtensionDefine> &externalExtensionDefs)
{
    LINGLONG_TRACE("resolve RuntimeLayer extension");

    const auto &info = layer.getCachedItem().info;
    if (info.extensions) {
        auto res = resolveExtension(layer, *info.extensions, info.channel, true);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    // merge external extensions
    if (!externalExtensionDefs.empty()) {
        return resolveExtension(layer, externalExtensionDefs, info.channel, true);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
RunContext::resolveExtension(RuntimeLayer &targetLayer,
                             const std::vector<api::types::v1::ExtensionDefine> &extDefs,
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
            if (skipOnNotFound) {
                continue;
            }
            return LINGLONG_ERR("extension is not installed", ref);
        }

        auto layer = RuntimeLayer::create(*ref, *this);
        if (!layer) {
            return LINGLONG_ERR(layer);
        }

        if (layer->getCachedItem().info.kind != "extension") {
            return LINGLONG_ERR(fmt::format("{} is not an extension", ref->toString()));
        }

        auto &extensionLayer = extensionLayers.emplace_back(std::move(layer).value());
        extensionLayer.setExtensionInfo(RuntimeLayer::ExtensionRuntimeLayerInfo{
          .extensionInfo = extDef,
          .extensionLayer = std::ref(extensionLayer),
          .forRef = targetLayer.getReference().toString(),
        });
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

std::vector<api::types::v1::ExtensionDefine> RunContext::matchedExtensionDefines(
  const package::Reference &ref,
  const std::optional<std::map<std::string, std::vector<api::types::v1::ExtensionDefine>>>
    &externalExtensionDefs)
{
    std::vector<api::types::v1::ExtensionDefine> result;

    if (externalExtensionDefs.has_value()) {
        for (const auto &[key, defs] : *externalExtensionDefs) {
            auto fuzzyRef = package::FuzzyReference::parse(key);
            if (!fuzzyRef) {
                LogE("invalid ref {}: {}", key, fuzzyRef.error());
                continue;
            }

            if (fuzzyRef->id != ref.id) {
                continue;
            }

            if (fuzzyRef->version) {
                if (!ref.version.semanticMatch(*fuzzyRef->version)) {
                    continue;
                }
            }

            result.insert(result.end(), defs.begin(), defs.end());
        }
    }

    return result;
}

void RunContext::detectDisplaySystem(generator::ContainerCfgBuilder &builder) noexcept
{
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

utils::error::Result<void> RunContext::fillContextCfg(
  linglong::generator::ContainerCfgBuilder &builder, const std::filesystem::path &bundlePath)
{
    LINGLONG_TRACE("fill ContainerCfgBuilder with run context");

    builder.setContainerId(containerID);
    if (contextCfg.timezone) {
        builder.setTimezone(*contextCfg.timezone);
    }

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    builder.setBasePath(baseLayer->getLayerDir()->filesDirPath());

    if (appOutput) {
        builder.setAppPath(*appOutput, false);
    } else {
        if (appLayer) {
            builder.setAppPath(appLayer->getLayerDir()->filesDirPath());
        }
    }

    if (runtimeOutput) {
        builder.setRuntimePath(*runtimeOutput, false);
    } else {
        if (runtimeLayer) {
            builder.setRuntimePath(runtimeLayer->getLayerDir()->filesDirPath());
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
        const auto &info = ext.getCachedItem().info;
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
          .source = ext.getLayerDir()->filesDirPath(),
          .type = "bind",
          .uidMappings = {},
        });
    }
    if (!extensionMounts.empty()) {
        builder.setExtensionMounts(extensionMounts);
    }

    auto res = fillExtraAppMounts(builder, bundlePath);
    if (!res) {
        return res;
    }

    if (!environment.empty()) {
        builder.appendEnv(environment);
    }

    detectDisplaySystem(builder);

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::fillExtraAppMounts(generator::ContainerCfgBuilder &builder,
                                                          const std::filesystem::path &bundlePath)
{
    LINGLONG_TRACE("fill extra app mounts");

    auto fillPermissionsBinds =
      [&builder, &bundlePath, this](RuntimeLayer &layer) -> utils::error::Result<void> {
        LINGLONG_TRACE("fill permissions binds");

        const auto &info = layer.getCachedItem().info;

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
              [&applicationMounts, &bundlePath](
                const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
                  std::filesystem::path source = bind.source;
                  applicationMounts.push_back(ocppi::runtime::config::types::Mount{
                    .destination = bind.destination,
                    .gidMappings = {},
                    .options = { { "rbind" } },
                    .source = bundlePath / "rootfs"
                      / (source.is_absolute() ? source.relative_path() : source),
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

    return state;
}

utils::error::Result<std::filesystem::path> RunContext::getBaseLayerPath() const
{
    LINGLONG_TRACE("get base layer path");

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    return baseLayer->getLayerDir()->path();
}

utils::error::Result<std::filesystem::path> RunContext::getRuntimeLayerPath() const
{
    LINGLONG_TRACE("get runtime layer path");

    if (!runtimeLayer) {
        return LINGLONG_ERR("no runtime layer exist");
    }

    return runtimeLayer->getLayerDir()->path();
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
