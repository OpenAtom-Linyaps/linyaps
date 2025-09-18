/* SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.  + *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "run_context.h"

#include "linglong/common/display.h"
#include "linglong/extension/extension.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/log/log.h"

namespace linglong::runtime {

RuntimeLayer::RuntimeLayer(package::Reference ref, RunContext &context)
    : reference(ref)
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

utils::error::Result<void> RuntimeLayer::resolveLayer(const QStringList &modules,
                                                      const std::optional<std::string> &subRef)
{
    LINGLONG_TRACE("resolve layer");

    auto &repo = runContext.get().getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    if (modules.isEmpty()) {
        layer = repo.getMergedModuleDir(reference);
    } else if (modules.size() > 1) {
        layer = repo.getMergedModuleDir(reference, modules);
        temporary = true;
    } else {
        layer = repo.getLayerDir(reference, modules[0].toStdString(), subRef);
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
            return LINGLONG_ERR("no cached item found: " + reference.toString());
        }
        cachedItem = std::move(item).value();
    }

    return *cachedItem;
}

RunContext::~RunContext()
{
    if (!bundle.empty()) {
        std::error_code ec;
        if (std::filesystem::remove_all(bundle, ec) == static_cast<std::uintmax_t>(-1)) {
            qWarning() << "failed to remove " << bundle.c_str() << ": " << ec.message().c_str();
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
        return LINGLONG_ERR("no cached item found: " + runnable.toString());
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
            auto runtimeFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(runtime));
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
    auto baseFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(baseRef));
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
        resolveExtension(*runtimeLayer);
    }

    // resolve base extension
    resolveExtension(*baseLayer);

    // all reference are cleard , we can get actual layer directory now
    QStringList appModules = options.appModules.value_or(QStringList{});
    return resolveLayer(options.depsBinaryOnly, appModules);
}

utils::error::Result<void> RunContext::resolve(const api::types::v1::BuilderProject &target,
                                               std::filesystem::path buildOutput)
{
    LINGLONG_TRACE("resolve RunContext from builder project "
                   + QString::fromStdString(target.package.id));

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

    auto baseFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(target.base));
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
        auto runtimeFuzzyRef =
          package::FuzzyReference::parse(QString::fromStdString(*target.runtime));
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
            return LINGLONG_ERR("no cached item found: " + runtimeLayer->getReference().toString());
        }

        auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(layer->info.base));
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
            return LINGLONG_ERR(QString{ "Base is not compatible with runtime. \n"
                                         "- Current base: %1\n"
                                         "- Current runtime: %2\n"
                                         "- Base required by runtime: %3" }
                                  .arg(baseLayer->getReference().toString(),
                                       runtimeLayer->getReference().toString(),
                                       layer->info.base.c_str()));
        }
    }

    return resolveLayer(false, {});
}

utils::error::Result<void> RunContext::resolveLayer(bool depsBinaryOnly,
                                                    const QStringList &appModules)
{
    LINGLONG_TRACE("resolve layers");

    std::optional<std::string> subRef;
    if (appLayer) {
        auto item = appLayer->getCachedItem();
        if (item && item->info.uuid) {
            subRef = item->info.uuid;
        }
    }

    QStringList depsModules;
    if (depsBinaryOnly) {
        depsModules << "binary";
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

    auto replaceSubstring = [](std::string_view str, std::string_view from, std::string_view to) {
        std::string result;
        if (from.empty()) {
            return std::string(str);
        }

        size_t start = 0;
        while (true) {
            size_t pos = str.find(from, start);
            if (pos == std::string_view::npos) {
                break;
            }
            // Append the part before the match
            result.append(str.data() + start, pos - start);
            // Append the replacement
            result.append(to.data(), to.size());
            // Move past the matched part
            start = pos + from.size();
        }
        // Append the remaining part of the string
        result.append(str.data() + start, str.size() - start);
        return result;
    };

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
        if (!extensionDefine.allowEnv) {
            continue;
        }
        const auto &allowEnv = *extensionDefine.allowEnv;

        auto extItem = ext.getCachedItem();
        if (!extItem) {
            continue;
        }
        const auto &extInfo = extItem->info;
        if (!extInfo.extImpl) {
            qWarning() << "no ext_impl found for " << ext.getReference().toString();
            continue;
        }
        const auto &extImpl = *extInfo.extImpl;
        if (!extImpl.env) {
            continue;
        }
        for (const auto &env : *extImpl.env) {
            auto allowed = allowEnv.find(env.first);
            if (allowed == allowEnv.end()) {
                qWarning() << "env " << QString::fromStdString(env.first) << " not allowed in "
                           << layer.get().getReference().toString();
                continue;
            }

            std::string res =
              replaceSubstring(env.second,
                               "$PREFIX",
                               "/opt/extensions/" + ext.getReference().id.toStdString());
            auto &value = environment[env.first];
            if (!value.empty()) {
                res = replaceSubstring(res, "$ORIGIN", value);
            } else if (!allowed->second.empty()) {
                res = replaceSubstring(res, "$ORIGIN", allowed->second);
            }
            value = res;
            qDebug() << "environment[" << QString::fromStdString(env.first)
                     << "]=" << QString::fromStdString(res);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RunContext::resolveExtension(RuntimeLayer &layer)
{
    LINGLONG_TRACE("resolve extension");

    auto item = layer.getCachedItem();
    if (!item) {
        return LINGLONG_ERR("no cached item found: " + layer.getReference().toString());
    }

    const auto &info = item->info;
    if (info.extensions) {
        for (const auto &extension : *info.extensions) {
            qDebug() << "handle extensions: " << QString::fromStdString(extension.name);
            qDebug() << "version: " << QString::fromStdString(extension.version);
            qDebug() << "directory: " << QString::fromStdString(extension.directory);
            if (extension.allowEnv) {
                for (const auto &allowEnv : *extension.allowEnv) {
                    qDebug() << "allowEnv: " << QString::fromStdString(allowEnv.first) << ":"
                             << QString::fromStdString(allowEnv.second);
                }
            }

            std::string name = extension.name;
            auto ext = extension::ExtensionFactory::makeExtension(name);
            if (!ext->shouldEnable(name)) {
                continue;
            }

            auto fuzzyRef =
              package::FuzzyReference::create(QString::fromStdString(info.channel),
                                              QString::fromStdString(name),
                                              QString::fromStdString(extension.version),
                                              std::nullopt);
            auto ref = repo.clearReference(*fuzzyRef,
                                           { .fallbackToRemote = false, .semanticMatching = true });
            if (!ref) {
                // extension is not installed, ignore it
                qDebug() << "extension is not installed: " << fuzzyRef->toString();
                continue;
            }

            auto &extensionLayer = extensionLayers.emplace_back(*ref, *this);
            extensionLayer.setExtensionInfo(
              std::make_pair(extension, std::reference_wrapper<RuntimeLayer>(extensionLayer)));
        }
    }

    return LINGLONG_OK;
}

void RunContext::detectDisplaySystem(generator::ContainerCfgBuilder &builder) noexcept
{
    LINGLONG_TRACE("detect display system");

    while (true) {
        auto *xOrgDisplayEnv = ::getenv("DISPLAY");
        if (xOrgDisplayEnv == nullptr || xOrgDisplayEnv[0] == '\0') {
            LogD("DISPLAY is not set, ignore it");
            break;
        }

        auto xOrgDisplay = common::getXOrgDisplay(xOrgDisplayEnv);
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

        auto xOrgAuthFile = common::getXOrgAuthFile(xOrgAuthFileEnv);
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

        auto waylandDisplay = common::getWaylandDisplay(waylandDisplayEnv);
        if (!waylandDisplay) {
            LogW("failed to get Wayland display: {}, ignore it", waylandDisplay.error());
            break;
        }

        builder.bindWaylandSocket(waylandDisplay.value());
        break;
    }
}

utils::error::Result<void>
RunContext::fillContextCfg(linglong::generator::ContainerCfgBuilder &builder)
{
    LINGLONG_TRACE("fill ContainerCfgBuilder with run context");

    builder.setContainerId(containerID);

    if (!baseLayer) {
        return LINGLONG_ERR("run context doesn't resolved");
    }

    auto bundleDir = runtime::makeBundleDir(containerID);
    if (!bundleDir) {
        return LINGLONG_ERR("failed to get bundle dir of " + QString::fromStdString(containerID));
    }
    bundle = *bundleDir;

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

    for (const auto &ext : extensionLayers) {
        std::string name = ext.getReference().id.toStdString();
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
    if (!extensionMounts.empty()) {
        builder.setExtensionMounts(extensionMounts);
    }

    builder.setBundlePath(bundle);

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
            qWarning() << "failed to apply permission binds for " << ext.getReference().toString();
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
        state.base = baseLayer->getReference().toString().toStdString();
    }

    if (appLayer) {
        state.app = appLayer->getReference().toString().toStdString();
    }

    if (runtimeLayer) {
        state.runtime = runtimeLayer->getReference().toString().toStdString();
    }

    state.extensions = std::vector<std::string>{};
    for (auto &ext : extensionLayers) {
        state.extensions->push_back(ext.getReference().toString().toStdString());
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
