// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ref_installation.h"

#include "linglong/package_manager/data_monitor.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/log/log.h"

#include <chrono>
#include <thread>

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
    , taskTotalSize(0)
    , taskNeededSize(0)
    , taskFetchedSize(0)
{
    taskName = fmt::format("Install {}", this->fuzzyRef.toString());
}

utils::error::Result<void> RefInstallationAction::doAction(PackageTask &task)
{
    LINGLONG_TRACE("ref installation do action");

    mainTask = &task;

    DataMonitor monitor(5, 1, [this](DataMonitor &m) {
        mainTask->updateMessage(
          fmt::format("{} {:>9}", taskMessage, fmt::format("[{}]", m.getHumanSpeed())));
    });

    QObject::connect(mainTask, &service::PackageTask::DataArrived, [this, &monitor](uint arrived) {
        monitor.dataArrived(arrived);
        monitor.start();

        taskFetchedSize += arrived;
        if (taskTotalSize > 0 && taskNeededSize > 0) {
            mainTask->updateProgress(taskFetchedSize * 100.0 / taskNeededSize);
        }
    });

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

utils::error::Result<void> RefInstallationAction::preInstall(Task &task)
{
    LINGLONG_TRACE("ref installation preInstall");

    task.updateState(linglong::api::types::v1::State::Processing,
                     fmt::format("Installing {} - Preparing...", fuzzyRef.id));

    auto extraOnly = extraModuleOnly(modules);
    auto localRef = repo.latestLocalReference(fuzzyRef);
    if (extraOnly) {
        if (!localRef) {
            return LINGLONG_ERR("no matched binary module found",
                                utils::error::ErrorCode::AppInstallModuleRequireAppFirst);
        }

        // filter out the modules that are already installed
        std::vector<std::string> toInstall;
        for (const auto &module : modules) {
            auto ret = repo.getLayerItem(*localRef, module);
            if (!ret) {
                toInstall.emplace_back(module);
            }
        }
        if (toInstall.empty()) {
            return LINGLONG_ERR("no modules to install",
                                utils::error::ErrorCode::AppInstallModuleAlreadyExists);
        }
        modules = std::move(toInstall);

        // Use the locally installed binary module's version to locate corresponding remote
        // modules
        fuzzyRef.version = localRef->version.toString();
    } else if (localRef && localRef->version.toString() == fuzzyRef.version) {
        // if the user-specified version exactly matches the locally installed version
        return LINGLONG_ERR("package already installed",
                            utils::error::ErrorCode::AppInstallAlreadyInstalled);
    }

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
        if (!pm.waitConfirm(*mainTask,
                            api::types::v1::InteractionMessageType::Upgrade,
                            additionalMessage)) {
            return LINGLONG_ERR("action canceled");
        }
    }

    this->operation = std::move(operation).value();
    this->candidates = std::move(candidates).value();

    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::install(Task &task)
{
    LINGLONG_TRACE("ref installation install");

    const auto &newRef = operation.newRef->reference;

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

    auto meta = repo.fetchRefMetaData(*operation.newRef, installModules.front(), true);
    if (!meta) {
        return LINGLONG_ERR(meta);
    }

    auto info = meta->getPackageInfo();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    std::vector<std::tuple<package::ReferenceWithRepo, std::string, repo::RefMetaData>>
      refsToInstall;
    refsToInstall.emplace_back(
      std::make_tuple(*operation.newRef, installModules.front(), std::move(meta).value()));

    auto gatherToInstallInfo = [this,
                                &refsToInstall](package::ReferenceWithRepo refRepo,
                                                std::string module) -> utils::error::Result<void> {
        LINGLONG_TRACE("gather to install info");
        auto meta = repo.fetchRefMetaData(refRepo, module);
        if (!meta) {
            return LINGLONG_ERR(meta);
        }

        refsToInstall.emplace_back(
          std::make_tuple(std::move(refRepo), std::move(module), std::move(meta).value()));
        return LINGLONG_OK;
    };

    for (auto it = installModules.begin() + 1; it != installModules.end(); ++it) {
        auto res = gatherToInstallInfo(*operation.newRef, *it);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    bool isApp = (info->kind == "app");
    if (isApp) {
        auto gatherDepsToInstall =
          [this, gatherToInstallInfo](
            const std::string &refStr,
            const std::optional<std::string> &channel) -> utils::error::Result<void> {
            LINGLONG_TRACE("gather to install info from deps");
            auto toInstall = pm.needToInstall(refStr, channel);
            if (!toInstall) {
                return LINGLONG_ERR(toInstall);
            }

            if (toInstall->has_value()) {
                auto res = gatherToInstallInfo(std::move(*toInstall).value(), "binary");
                if (!res) {
                    return LINGLONG_ERR(res);
                }
            }

            return LINGLONG_OK;
        };

        auto gatherAppDepsToInstall =
          [gatherDepsToInstall](
            const api::types::v1::PackageInfoV2 &info) -> utils::error::Result<void> {
            LINGLONG_TRACE("gather app deps to install info");
            auto res = gatherDepsToInstall(info.base, info.channel);
            if (!res) {
                return LINGLONG_ERR(res);
            }

            if (info.runtime) {
                res = gatherDepsToInstall(*info.runtime, info.channel);
                if (!res) {
                    return LINGLONG_ERR(res);
                }
            }

            return LINGLONG_OK;
        };

        auto res = gatherAppDepsToInstall(*info);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    for (const auto &ref : refsToInstall) {
        const auto &[refRepo, module, meta] = ref;
        auto stat = repo.getRefStatistics(meta);
        if (!stat) {
            LogW("failed to get stat {}", stat.error());
            continue;
        }

        taskTotalSize += stat->archived;
        taskNeededSize += stat->needed_archived;
    }

    LogD("install total size {}, need download size {}", taskTotalSize, taskNeededSize);

    utils::Transaction transaction;
    // uninstall target ref if failed
    transaction.addRollBack([this]() noexcept {
        auto res = pm.tryUninstallRef(operation.newRef->reference);
        if (!res) {
            LogW("failed to roll back installed {}: {}",
                 operation.newRef->reference.toString(),
                 res.error());
        }
    });
    for (const auto &ref : refsToInstall) {
        const auto &[refRepo, module, meta] = ref;

        taskMessage = fmt::format("Installing {}/{}", refRepo.reference.toString(), module);
        task.updateMessage(taskMessage);

        auto res = pm.installRefModule(task, refRepo, module);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    if (isApp) {
        auto res = postInstallApp(task);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::postInstallApp([[maybe_unused]] Task &task)
{
    LINGLONG_TRACE("post install app");

    auto &newRef = operation.newRef->reference;
    auto &oldRef = operation.oldRef;

    auto res = oldRef ? pm.switchAppVersion(*oldRef, newRef, true) : pm.applyApp(newRef);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> RefInstallationAction::postInstall(Task &task)
{
    LINGLONG_TRACE("ref installation postInstall");

    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet) {
        LogE("failed to merge modules: {}", mergeRet.error());
    }

    auto &repo = operation.newRef->repo;
    task.updateState(linglong::api::types::v1::State::Succeed,
                     fmt::format("Install {} (from repo: {}) success",
                                 operation.newRef->reference.toString(),
                                 repo.alias.value_or(repo.name)));

    return LINGLONG_OK;
}

} // namespace linglong::service
