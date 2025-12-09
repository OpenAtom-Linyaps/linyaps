// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "uab_installation.h"

#include "linglong/utils/log/log.h"

namespace linglong::service {

std::shared_ptr<UabInstallationAction> UabInstallationAction::create(
  int uabFD, PackageManager &pm, repo::OSTreeRepo &repo, api::types::v1::CommonOptions opts)
{
    auto p = new UabInstallationAction(uabFD, pm, repo, std::move(opts));
    return std::shared_ptr<UabInstallationAction>(p);
}

UabInstallationAction::CheckedLayers
splitUABLayers(std::vector<linglong::api::types::v1::UabLayer> layers)
{
    auto it =
      std::partition(layers.begin(), layers.end(), [](const api::types::v1::UabLayer &layer) {
          return layer.info.kind == "app";
      });
    std::vector<api::types::v1::UabLayer> otherLayers{ it, layers.end() };
    layers.erase(it, layers.end());

    return std::make_pair(std::move(layers), std::move(otherLayers));
}

// executable mode includes app layers and optional runtime layers
utils::error::Result<UabInstallationAction::CheckedLayers>
UabInstallationAction::checkExecModeUABLayers(
  repo::OSTreeRepo &repo, const std::vector<linglong::api::types::v1::UabLayer> &layers)
{
    LINGLONG_TRACE("check exec mode uab layers");

    auto splitLayers = splitUABLayers(layers);
    const auto &appLayers = splitLayers.first;
    const auto &otherLayers = splitLayers.second;

    if (appLayers.empty()) {
        return LINGLONG_ERR("no app layers found");
    }

    const auto &appInfo = appLayers.front().info;
    if (appInfo.runtime) {
        if (otherLayers.empty()) {
            return LINGLONG_ERR("runtime layer not found");
        }

        auto runtimeRef = package::Reference::fromPackageInfo(otherLayers.front().info);
        if (!runtimeRef) {
            return LINGLONG_ERR(runtimeRef);
        }

        auto fuzzyRef = package::FuzzyReference::parse(*appInfo.runtime);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }

        if (fuzzyRef->id != runtimeRef->id || appInfo.channel != runtimeRef->channel) {
            return LINGLONG_ERR("runtime layer not matched");
        }

        if (fuzzyRef->version) {
            if (!runtimeRef->version.semanticMatch(*fuzzyRef->version)) {
                return LINGLONG_ERR("runtime layer version not matched");
            }
        }
    }

    if (auto res = checkUABLayersConstrain(repo, appLayers); !res) {
        return LINGLONG_ERR(res);
    }

    if (auto res = checkUABLayersConstrain(repo, otherLayers); !res) {
        return LINGLONG_ERR(res);
    }

    return splitLayers;
}

// distribution mode includes one or more module layers from a single package
utils::error::Result<UabInstallationAction::CheckedLayers>
UabInstallationAction::checkDistributionModeUABLayers(
  repo::OSTreeRepo &repo, const std::vector<linglong::api::types::v1::UabLayer> &layers)
{
    LINGLONG_TRACE("check distribution mode uab layers");

    auto splitLayers = splitUABLayers(layers);
    const auto &appLayers = splitLayers.first;
    const auto &otherLayers = splitLayers.second;

    if (appLayers.empty() && otherLayers.empty()) {
        return LINGLONG_ERR("no layers found");
    }

    if (!appLayers.empty() && !otherLayers.empty()) {
        return LINGLONG_ERR("layers from multiple packages found");
    }

    if (auto res = checkUABLayersConstrain(repo, appLayers); !res) {
        return LINGLONG_ERR(res);
    }
    if (auto res = checkUABLayersConstrain(repo, otherLayers); !res) {
        return LINGLONG_ERR(res);
    }

    return splitLayers;
}

utils::error::Result<void> UabInstallationAction::checkUABLayersConstrain(
  repo::OSTreeRepo &repo, const std::vector<linglong::api::types::v1::UabLayer> &layers)
{
    LINGLONG_TRACE("check uab layers constrain");

    auto currentArch = package::Architecture::currentCPUArchitecture();
    if (!currentArch) {
        return LINGLONG_ERR(currentArch);
    }

    if (layers.empty()) {
        return LINGLONG_OK;
    }

    const auto &front = layers.front().info;
    for (const auto &layer : layers) {
        auto arch = package::Architecture::parse(layer.info.arch[0]);
        if (!arch) {
            return LINGLONG_ERR(arch);
        }
        if (*arch != *currentArch) {
            return LINGLONG_ERR(
              fmt::format("uab arch: {} not match host architecture", layer.info.arch[0]));
        }

        if (layer.info.id != front.id) {
            return LINGLONG_ERR("more than one layers with different id");
        }

        if (layer.info.version != front.version) {
            return LINGLONG_ERR("modules have different version");
        }
    }

    if (extraModuleOnly(layers)) {
        auto fuzzyRef =
          package::FuzzyReference::create(front.channel, front.id, front.version, std::nullopt);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }

        auto localRef = repo.clearReference(*fuzzyRef,
                                            {
                                              .forceRemote = false,
                                              .fallbackToRemote = false,
                                              .semanticMatching = false,
                                            });

        auto version = package::Version::parse(front.version);
        if (!version) {
            return LINGLONG_ERR(version);
        }
        if (!localRef || localRef->version != version) {
            return LINGLONG_ERR("no matched binary module found");
        }
    }

    return LINGLONG_OK;
}

bool UabInstallationAction::extraModuleOnly(const std::vector<api::types::v1::UabLayer> &layers)
{
    for (const auto &layer : layers) {
        const auto &module = layer.info.packageInfoV2Module;
        if (module == "binary" || module == "runtime") {
            return false;
        }
    }
    return true;
}

UabInstallationAction::UabInstallationAction(int uabFD,
                                             PackageManager &pm,
                                             repo::OSTreeRepo &repo,
                                             api::types::v1::CommonOptions opts)
    : Action(pm, repo, opts)
    , fd(dup(uabFD))
{
}

UabInstallationAction::~UabInstallationAction()
{
    close(fd);
}

utils::error::Result<void> UabInstallationAction::prepare()
{
    LINGLONG_TRACE("uab installation prepare");

    if (this->uabFile) {
        return LINGLONG_OK;
    }

    auto uabFileRet = package::UABFile::loadFromFile(fd);
    if (!uabFileRet) {
        return LINGLONG_ERR(fmt::format("failed to load uab file from fd {}", fd), uabFileRet);
    }
    auto uabFile = std::move(uabFileRet).value();

    auto res = uabFile->verify();
    if (!res) {
        return LINGLONG_ERR(res);
    }
    if (!*res) {
        return LINGLONG_ERR("failed to verify uab file");
    }

    auto metaInfoRet = uabFile->getMetaInfo();
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet);
    }
    const auto &metaInfo = metaInfoRet->get();

    if (metaInfo.onlyApp && *metaInfo.onlyApp) {
        auto res = checkExecModeUABLayers(repo, metaInfo.layers);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        checkedLayers = std::move(res).value();
    } else {
        auto res = checkDistributionModeUABLayers(repo, metaInfo.layers);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        checkedLayers = std::move(res).value();
    }

    this->taskName = fmt::format("installing uab");
    this->uabFile = std::move(uabFile);

    return LINGLONG_OK;
}

utils::error::Result<void> UabInstallationAction::doAction(PackageTask &task)
{
    LINGLONG_TRACE("uab installation action");

    if (!uabFile) {
        return LINGLONG_ERR("action not prepared");
    }

    auto ret = preInstall(task);
    if (!ret) {
        return ret;
    }

    ret = install(task);
    if (!ret) {
        return ret;
    }

    return postInstall(task);
}

utils::error::Result<void> UabInstallationAction::preInstall(PackageTask &task)
{
    LINGLONG_TRACE("uab installation preInstall");

    task.updateState(linglong::api::types::v1::State::Processing, "installing uab");

    const auto &toCheck = checkedLayers.first.empty() ? checkedLayers.second : checkedLayers.first;
    auto operation = getActionOperation(toCheck.front().info, extraModuleOnly(toCheck));
    if (!operation) {
        return LINGLONG_ERR(operation);
    }

    task.updateProgress(5);

    if (operation->operation == ActionOperation::Overwrite) {
        return LINGLONG_ERR("package already installed",
                            utils::error::ErrorCode::AppInstallAlreadyInstalled);
    }

    if (operation->operation == ActionOperation::Downgrade && !options.force) {
        return LINGLONG_ERR("latest version already installed",
                            utils::error::ErrorCode::AppInstallNeedDowngrade);
    }

    if (operation->operation == ActionOperation::Upgrade && !options.skipInteraction) {
        auto additionalMessage = api::types::v1::PackageManager1RequestInteractionAdditionalMessage{
            .localRef = operation->oldRef->toString(),
            .remoteRef = operation->newRef->reference.toString()
        };
        if (!pm.waitConfirm(task,
                            api::types::v1::InteractionMessageType::Upgrade,
                            additionalMessage)) {
            return LINGLONG_ERR("action canceled");
        }
    }

    this->operation = std::move(operation).value();

    return LINGLONG_OK;
}

utils::error::Result<void> UabInstallationAction::install([[maybe_unused]] PackageTask &task)
{
    LINGLONG_TRACE("uab installation install");

    task.updateProgress(10);

    auto mountPoint = uabFile->unpack();
    if (!mountPoint) {
        return LINGLONG_ERR(mountPoint);
    }
    uabMountPoint = std::move(mountPoint).value();

    task.updateProgress(15);

    const auto &metaInfo = uabFile->getMetaInfo()->get();
    if (metaInfo.onlyApp && *metaInfo.onlyApp) {
        return installExecModeUAB(task);
    } else {
        return installDistributionModeUAB(task);
    }
}

utils::error::Result<void> UabInstallationAction::postInstall(PackageTask &task)
{
    LINGLONG_TRACE("uab installation postInstall");

    const auto &newRef = operation.newRef->reference;
    const auto &oldRef = operation.oldRef;

    auto res = repo.mergeModules();
    if (!res) {
        LogE("merge modules failed: {}", res.error());
    }

    auto ret = pm.executePostInstallHooks(newRef);
    if (!ret) {
        task.reportError(std::move(ret).error());
        return LINGLONG_ERR("failed to execute post install hooks");
    }

    if (operation.kind == "app") {
        auto res = oldRef ? pm.switchAppVersion(*oldRef, newRef, true) : pm.applyApp(newRef);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    transaction.commit();
    task.updateState(linglong::api::types::v1::State::Succeed, "install uab successfully");

    return LINGLONG_OK;
}

utils::error::Result<void> UabInstallationAction::installUabLayer(
  const std::vector<api::types::v1::UabLayer> &layers, std::optional<std::string> subRef)
{
    LINGLONG_TRACE("install uab layers from single package");

    for (const auto &layer : layers) {
        std::error_code ec;
        auto layerDirPath =
          uabMountPoint / "layers" / layer.info.id / layer.info.packageInfoV2Module;
        if (!std::filesystem::exists(layerDirPath, ec)) {
            if (ec) {
                auto msg = fmt::format("get status of {} failed: {}", layerDirPath, ec.message());
                return LINGLONG_ERR(msg);
            }

            auto msg = fmt::format("layer directory {} doesn't exist", layerDirPath);
            return LINGLONG_ERR(msg);
        }

        std::vector<std::filesystem::path> overlays;
        auto signPath = uabFile->extractSignData();
        if (!signPath) {
            return LINGLONG_ERR(signPath);
        }
        if (!signPath->empty()) {
            overlays.emplace_back(std::move(signPath).value());
        }

        auto ref = package::Reference::fromPackageInfo(layer.info);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }

        auto ret =
          this->repo.importLayerDir(package::LayerDir{ layerDirPath.c_str() }, overlays, subRef);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        std::for_each(overlays.begin(), overlays.end(), [](const std::filesystem::path &dir) {
            std::error_code ec;
            if (std::filesystem::remove_all(dir, ec) == static_cast<std::uintmax_t>(-1) && ec) {
                LogW("failed to remove temporary directory {}", dir);
            }
        });

        transaction.addRollBack([this,
                                 ref = std::move(ref).value(),
                                 module = layer.info.packageInfoV2Module,
                                 subRef]() noexcept {
            auto ret = this->repo.remove(ref, module, subRef);
            if (!ret) {
                LogE("rollback importLayerDir failed: {}", ret.error());
            }

            ret = pm.executePostUninstallHooks(ref);
            if (!ret) {
                LogE("failed to rollback execute uninstall hooks: {}", ret.error());
            }
        });
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UabInstallationAction::installExecModeUAB(PackageTask &task)
{
    LINGLONG_TRACE("install exec mode uab");

    const auto &appLayers = checkedLayers.first;
    const auto &appInfo = appLayers.front().info;

    auto res = pm.installDependsRef(task, appInfo.base, appInfo.channel);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    task.updateProgress(25);

    if (appInfo.runtime) {
        const auto &otherLayers = checkedLayers.second;
        auto fuzzyRef = package::FuzzyReference::parse(*appInfo.runtime);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }

        auto satisfiedRef = repo.latestLocalReference(*fuzzyRef);
        // no compatible runtime found in local, install the one from the UAB file, the
        // runtime is identified by a uuid, so it is exclusively usable by the currently
        // installed application
        if (!satisfiedRef) {
            auto metaInfo = uabFile->getMetaInfo()->get();
            auto res = installUabLayer(otherLayers, metaInfo.uuid);
            if (!res) {
                return LINGLONG_ERR(res);
            }
        }
    }

    task.updateProgress(35);

    res = installUabLayer(appLayers);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UabInstallationAction::installDistributionModeUAB(PackageTask &task)
{
    LINGLONG_TRACE("install distribution mode uab");

    if (!checkedLayers.first.empty()) {
        const auto &appInfo = checkedLayers.first.front().info;
        auto res = pm.installAppDepends(task, appInfo);
        if (!res) {
            return LINGLONG_ERR(res);
        }

        task.updateProgress(25);

        res = installUabLayer(checkedLayers.first);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    task.updateProgress(80);

    if (!checkedLayers.second.empty()) {
        auto res = installUabLayer(checkedLayers.second);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    return LINGLONG_OK;
}

} // namespace linglong::service
