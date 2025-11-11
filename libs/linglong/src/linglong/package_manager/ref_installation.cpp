// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ref_installation.h"

#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/repo/repo_cache.h"
#include "linglong/utils/log/log.h"

namespace linglong::service {

std::shared_ptr<RefInstallationAction>
RefInstallationAction::create(package::FuzzyReference fuzzyRef,
                              std::vector<std::string> modules,
                              PackageManager &pm,
                              repo::OSTreeRepo &repo,
                              api::types::v1::CommonOptions opts,
                              std::optional<api::types::v1::Repo> usedRepo)
{
    auto p =
      new RefInstallationAction(fuzzyRef, modules, pm, repo, std::move(opts), std::move(usedRepo));
    return std::shared_ptr<RefInstallationAction>(p);
}

void RefInstallationAction::checkModules(const std::vector<std::string> &modules,
                                         bool &hasBinary,
                                         bool &extraModule)
{
    for (const auto &module : modules) {
        if (module == "binary" || module == "runtime") {
            hasBinary = true;
        } else {
            extraModule = true;
        }
    }
}

bool RefInstallationAction::extraModuleOnly(const std::vector<std::string> &modules)
{
    bool hasBinary = false;
    bool extraModule = false;
    checkModules(modules, hasBinary, extraModule);

    return extraModule && !hasBinary;
}

RefInstallationAction::RefInstallationAction(package::FuzzyReference fuzzyRef,
                                             std::vector<std::string> modules,
                                             PackageManager &pm,
                                             repo::OSTreeRepo &repo,
                                             api::types::v1::CommonOptions opts,
                                             std::optional<api::types::v1::Repo> usedRepo)
    : Action(pm, repo, opts)
    , fuzzyRef(std::move(fuzzyRef))
    , modules(std::move(modules))
    , usedRepo(std::move(usedRepo))
{
}

utils::error::Result<void> RefInstallationAction::prepare()
{
    LINGLONG_TRACE("ref installation prepare");

    if (prepared) {
        return LINGLONG_OK;
    }

    auto extraOnly = extraModuleOnly(modules);
    auto localRef = repo.latestLocalReference(fuzzyRef);
    if (extraOnly) {
        if (!localRef) {
            return LINGLONG_ERR("no matched binary module found");
        }

        // Use the locally installed binary module's version to locate corresponding remote
        // modules
        fuzzyRef.version = localRef->version.toString();

        // filter out the modules that are already installed
        std::vector<std::string> toInstall;
        for (const auto &module : modules) {
            auto ret = repo.getLayerItem(*localRef, module);
            if (!ret) {
                toInstall.emplace_back(module);
            }
        }
        modules = std::move(toInstall);
    } else if (localRef && localRef->version.toString() == fuzzyRef.version) {
        // if the user-specified version exactly matches the locally installed version
        return LINGLONG_ERR("package already installed",
                            utils::error::ErrorCode::AppInstallAlreadyInstalled);
    }

    this->taskName = fmt::format("Installing {}", fuzzyRef.toString());
    this->extraOnly = extraOnly;

    prepared = true;
    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::doAction(PackageTask &task)
{
    LINGLONG_TRACE("ref installation do action");

    if (!prepared) {
        return LINGLONG_ERR("action not prepared");
    }

    auto res = preInstall(task);
    if (!res) {
        return res;
    }

    res = install(task);
    if (!res) {
        return res;
    }

    return postInstall(task);
}

utils::error::Result<void> RefInstallationAction::preInstall(PackageTask &task)
{
    LINGLONG_TRACE("ref installation preInstall");

    // find all candidate remote packages
    auto candidates = repo.matchRemoteByPriority(fuzzyRef, usedRepo);
    if (!candidates) {
        return LINGLONG_ERR(candidates);
    }

    // find the latest package from candidates
    auto target = candidates->getLatestPackage();
    if (!target) {
        return LINGLONG_ERR("package not found",
                            utils::error::ErrorCode::AppInstallNotFoundFromRemote);
    }

    auto operation = getActionOperation(target->second.get(), extraOnly);
    if (!operation) {
        return LINGLONG_ERR(operation);
    }
    if (operation->newRef) {
        operation->newRef->repo = target->first.get();
    }

    if (operation->operation == ActionOperation::Overwrite) {
        return LINGLONG_ERR("package already installed",
                            utils::error::ErrorCode::AppInstallAlreadyInstalled);
    }
    if (operation->operation == ActionOperation::Downgrade && !options.force) {
        return LINGLONG_ERR("latest version already installed",
                            utils::error::ErrorCode::AppInstallNeedDowngrade);
    }

    if (operation->operation == ActionOperation::Policy::Upgrade && !options.skipInteraction) {
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
    this->candidates = std::move(candidates).value();

    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::install(PackageTask &task)
{
    LINGLONG_TRACE("ref installation install");

    const auto &newRef = operation.newRef->reference;
    task.updateState(linglong::api::types::v1::State::Processing,
                     fmt::format("Installing {}", newRef.toString()));

    auto remoteModules = candidates.getReferenceModules(newRef);
    if (remoteModules.empty()) {
        return LINGLONG_ERR("no modules found");
    }

    auto installModules = std::vector<std::string>{};
    for (const auto &module : modules) {
        if (std::find(remoteModules.begin(), remoteModules.end(), module) != remoteModules.end()) {
            installModules.emplace_back(module);
            continue;
        }

        // install runtime module if binary module is not found
        if (module == "binary"
            && std::find(remoteModules.begin(), remoteModules.end(), "runtime")
              != remoteModules.end()) {
            installModules.emplace_back("runtime");
            continue;
        }
    }
    if (installModules.empty()) {
        return LINGLONG_ERR("no modules found");
    }

    pm.InstallRef(task, newRef, installModules, operation.newRef->repo);
    if (task.isTaskDone()) {
        return LINGLONG_ERR("install canceled");
    }
    transaction.addRollBack([this, &installModules, &newRef]() noexcept {
        auto tmp = PackageTask::createTemporaryTask();
        pm.UninstallRef(tmp, newRef, installModules);
        if (tmp.state() != linglong::api::types::v1::State::Succeed) {
            LogE("failed to rollback install {}", newRef.toString());
        }

        auto result = pm.executePostUninstallHooks(newRef);
        if (!result) {
            LogE("failed to rollback execute uninstall hooks: {}", result.error());
        }
    });

    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::postInstall(PackageTask &task)
{
    LINGLONG_TRACE("ref installation postInstall");

    task.updateSubState(linglong::api::types::v1::SubState::PostAction, "processing after install");

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet) {
        LogE("failed to merge modules: {}", mergeRet.error());
    }

    const auto &newRef = operation.newRef->reference;
    const auto &oldRef = operation.oldRef;

    auto ret = pm.executePostInstallHooks(newRef);
    if (!ret) {
        task.updateState(linglong::api::types::v1::State::Failed,
                         "Failed to execute postInstall hooks.\n" + ret.error().message());
        return LINGLONG_ERR(ret);
    }

    auto layer = this->repo.getLayerItem(newRef);
    if (!layer) {
        task.reportError(
          LINGLONG_ERRV(layer.error().message(), utils::error::ErrorCode::AppInstallFailed));
        return LINGLONG_ERR("failed to get layer item", layer);
    }
    // only app should do 'remove' and 'export'
    if (layer->info.kind == "app") {
        // remove all previous modules
        if (oldRef) {
            auto ret = pm.removeAfterInstall(*oldRef, newRef, modules);
            if (!ret) {
                auto msg = fmt::format("Failed to remove old reference {} after install {}: {}",
                                       oldRef->toString(),
                                       newRef.toString(),
                                       ret.error().message());

                task.reportError(LINGLONG_ERRV(msg, utils::error::ErrorCode::AppInstallFailed));
                return LINGLONG_ERR(ret);
            }
        } else {
            this->repo.exportReference(newRef);
        }
        auto result = pm.tryGenerateCache(newRef);
        if (!result) {
            task.reportError(
              LINGLONG_ERRV("Failed to generate some cache.\n" + result.error().message(),
                            utils::error::ErrorCode::AppInstallFailed));
            return LINGLONG_ERR(result);
        }
    }

    transaction.commit();
    task.updateState(linglong::api::types::v1::State::Succeed,
                     fmt::format("Install {} (from repo: {}) success",
                                 newRef.toString(),
                                 operation.newRef->repo.name));

    return LINGLONG_OK;
}

} // namespace linglong::service
