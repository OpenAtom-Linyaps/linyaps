// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_update.h"

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/extension/extension.h"
#include "linglong/package_manager/data_monitor.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/log/log.h"

namespace linglong::service {

std::shared_ptr<PackageUpdateAction>
PackageUpdateAction::create(std::vector<api::types::v1::PackageManager1Package> toUpgrade,
                            bool appOnly,
                            bool depsOnly,
                            PackageManager &pm,
                            repo::OSTreeRepo &repo)
{
    auto p = new PackageUpdateAction(std::move(toUpgrade), appOnly, depsOnly, pm, repo);
    return std::shared_ptr<PackageUpdateAction>(p);
}

PackageUpdateAction::PackageUpdateAction(
  std::vector<api::types::v1::PackageManager1Package> toUpgrade,
  bool appOnly,
  bool depsOnly,
  PackageManager &pm,
  repo::OSTreeRepo &repo)
    : Action(pm, repo, api::types::v1::CommonOptions{})
    , toUpgrade(std::move(toUpgrade))
    , appOnly(appOnly)
    , depsOnly(depsOnly)
    , taskTotalSize(0)
    , taskNeededSize(0)
    , taskFetchedSize(0)
{
}

utils::error::Result<void> PackageUpdateAction::prepare()
{
    LINGLONG_TRACE("package update prepare");

    if (prepared) {
        return LINGLONG_OK;
    }

    // check if the app is already installed
    auto apps = repo.listLocalApps();
    if (!apps) {
        return LINGLONG_ERR(apps.error());
    }

    if (!toUpgrade.empty()) {
        for (const auto &ref : toUpgrade) {
            auto it = std::find_if(apps->begin(), apps->end(), [&ref](const auto &app) {
                return app.id == ref.id && app.channel == ref.channel;
            });
            if (it != apps->end()) {
                appsToUpgrade.emplace_back(*it);
            }
        }
    } else {
        appsToUpgrade = std::move(apps).value();
    }

    if (appsToUpgrade.empty()) {
        return LINGLONG_ERR("No apps to upgrade", utils::error::ErrorCode::AppUpgradeLocalNotFound);
    }

    prepared = true;
    taskName = fmt::format("update apps");
    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::doAction(PackageTask &task)
{
    LINGLONG_TRACE("package update action");

    task.updateState(linglong::api::types::v1::State::Processing, "Updating applications");

    if (!prepared) {
        return LINGLONG_ERR("action not prepared");
    }

    auto res = update(task);
    if (!res) {
        return LINGLONG_ERR(res.error());
    }

    return postUpdate(task);
}

utils::error::Result<void> PackageUpdateAction::update(PackageTask &task)
{
    LINGLONG_TRACE("package update");

    DataMonitor monitor(5, 1, [this, &task](DataMonitor &m) {
        task.updateMessage(
          fmt::format("{} {:>9}", taskMessage, fmt::format("[{}]", m.getHumanSpeed())));
    });

    QObject::connect(&task,
                     &service::PackageTask::DataArrived,
                     [this, &task, &monitor](uint arrived) {
                         monitor.dataArrived(arrived);
                         monitor.start();
                         monitor.pause(false);

                         if (taskTotalSize > 0 && taskNeededSize > 0) {
                             taskFetchedSize += arrived;
                             task.updateProgress(taskFetchedSize * 100.0 / taskNeededSize);
                         }
                     });

    bool allFailed = true;
    for (const auto &app : appsToUpgrade) {
        if (task.isTaskDone()) {
            return LINGLONG_ERR("task was cancelled");
        }

        auto res = updateApp(task, app, appOnly, depsOnly);
        if (!res) {
            LogW("failed to update app {}: {}", app.id, res.error());
            continue;
        }
        monitor.pause(true);
        allFailed = false;
    }

    if (allFailed) {
        return LINGLONG_ERR("all apps failed to upgrade",
                            utils::error::ErrorCode::AppUpgradeFailed);
    }

    task.updateState(linglong::api::types::v1::State::Succeed, "Update applications success");

    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::postUpdate([[maybe_unused]] Task &task)
{
    LINGLONG_TRACE("package update postUpdate");

    auto res = repo.mergeModules();
    if (!res) {
        LogE("failed to merge modules: {}", res.error());
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::updateApp(Task &task,
                                                          const api::types::v1::PackageInfoV2 &app,
                                                          bool appOnly,
                                                          bool depsOnly)
{
    LINGLONG_TRACE(
      fmt::format("update app: {} appOnly: {} depsOnly: {}", app.id, appOnly, depsOnly));

    // reset task status
    taskTotalSize = 0;
    taskNeededSize = 0;
    taskFetchedSize = 0;

    taskMessage = fmt::format("Checking for updates {}", app.id);
    task.resetProgress(taskMessage);

    auto localRef = package::Reference::fromPackageInfo(app);
    if (!localRef) {
        return LINGLONG_ERR(localRef);
    }

    RefsToInstall refsToInstall;

    if (!depsOnly) {
        auto fuzzyRef =
          package::FuzzyReference::create(app.channel, app.id, std::nullopt, std::nullopt);
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }

        std::optional<package::Reference> local = std::move(localRef).value();
        auto res = gatherRefsToUpdate(refsToInstall, *fuzzyRef, local);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    std::optional<api::types::v1::PackageInfoV2> newAppInfo;
    if (!refsToInstall.empty()) {
        auto info = refsToInstall.front().second.front().second.getPackageInfo();
        if (!info) {
            return LINGLONG_ERR(info);
        }
        newAppInfo = std::move(info).value();
    }

    if (!appOnly) {
        auto res = gatherAppDepsToUpgrade(refsToInstall, newAppInfo ? newAppInfo.value() : app);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    for (const auto &[refRepo, modules] : refsToInstall) {
        for (const auto &[module, meta] : modules) {
            auto stat = repo.getRefStatistics(meta);
            if (!stat) {
                LogW("failed to get stat {}", stat.error());
                continue;
            }

            taskTotalSize += stat->archived;
            taskNeededSize += stat->needed_archived;
        }
    }

    LogD("update total size {}, need download size {}", taskTotalSize, taskNeededSize);

    utils::Transaction transaction;
    if (newAppInfo) {
        // uninstall target ref if failed
        transaction.addRollBack([this, &ref = refsToInstall.front().first.reference]() noexcept {
            auto res = pm.tryUninstallRef(ref);
            if (!res) {
                LogW("failed to roll back updated {}: {}", ref.toString(), res.error());
            }
        });
    }
    for (const auto &[refRepo, modules] : refsToInstall) {
        for (const auto &[module, meta] : modules) {
            taskMessage = fmt::format("Updating {}/{}", refRepo.reference.toString(), module);
            task.updateMessage(taskMessage);
            auto res = pm.installRefModule(task, refRepo, module);
            if (!res) {
                return LINGLONG_ERR(res);
            }
        }
    }

    if (!depsOnly && newAppInfo) {
        auto res = postUpdateApp(task, *localRef, refsToInstall.front().first);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<void>
PackageUpdateAction::postUpdateApp([[maybe_unused]] Task &task,
                                   const package::Reference &localRef,
                                   const package::ReferenceWithRepo &remoteRef)
{
    LINGLONG_TRACE("post update app");

    auto res = pm.switchAppVersion(localRef, remoteRef.reference, true);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
PackageUpdateAction::gatherRefsToUpdate(RefsToInstall &refsToInstall,
                                        const package::FuzzyReference &fuzzyRef,
                                        std::optional<package::Reference> &local,
                                        bool installIfMissing)
{
    LINGLONG_TRACE("gather refs to update");

    LogD("needToUpgrade {}", fuzzyRef.toString());

    auto res = pm.needToUpgrade(fuzzyRef, local, installIfMissing);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (res->has_value()) {
        const auto &[remoteRef, modules] = res->value();
        std::vector<std::pair<std::string, repo::RefMetaData>> modulePairs;
        // fetch package info only once for the same ref
        bool fetchPackageInfo = true;
        for (const auto &module : modules) {
            auto meta = repo.fetchRefMetaData(remoteRef, module, fetchPackageInfo);
            if (!meta) {
                return LINGLONG_ERR(meta);
            }
            modulePairs.emplace_back(module, std::move(meta).value());
            fetchPackageInfo = false;
        }
        refsToInstall.emplace_back(remoteRef, std::move(modulePairs));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::gatherExtensionsToUpdate(
  RefsToInstall &refsToInstall, const api::types::v1::PackageInfoV2 &info)
{
    LINGLONG_TRACE("gather extensions to upgrade info");

    if (!info.extensions) {
        return LINGLONG_OK;
    }

    for (const auto &extension : *info.extensions) {
        std::string name = extension.name;
        auto ext = extension::ExtensionFactory::makeExtension(name);
        if (!ext->shouldEnable(name)) {
            continue;
        }
        auto res = gatherDepsToUpdate(refsToInstall,
                                      fmt::format("{}/{}", name, extension.version),
                                      info.channel,
                                      true);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::gatherDepsToUpdate(RefsToInstall &refsToInstall,
                                                                   const std::string &refStr,
                                                                   const std::string &channel,
                                                                   bool isExtension)
{
    LINGLONG_TRACE("gather to upgrade info from deps");

    auto fuzzyRef = package::FuzzyReference::parse(refStr);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef);
    }

    if (!channel.empty() && !fuzzyRef->channel) {
        fuzzyRef->channel = channel;
    }

    std::optional<package::Reference> local;
    RefsToInstall tmp;
    auto res = gatherRefsToUpdate(tmp, *fuzzyRef, local, !isExtension);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (!isExtension) {
        api::types::v1::PackageInfoV2 info;
        if (!tmp.empty()) {
            auto res = tmp.front().second.front().second.getPackageInfo();
            if (!res) {
                return LINGLONG_ERR(res);
            }
            info = std::move(res).value();
        } else {
            if (local) {
                auto layer = repo.getLayerItem(*local);
                if (!layer) {
                    return LINGLONG_ERR(layer);
                }

                info = layer->info;
            }
        }

        auto res = gatherExtensionsToUpdate(tmp, info);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    std::move(tmp.begin(), tmp.end(), std::back_inserter(refsToInstall));
    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::gatherAppDepsToUpgrade(
  RefsToInstall &refsToInstall, const api::types::v1::PackageInfoV2 &info)
{
    LINGLONG_TRACE("gather app deps to upgrade info");

    auto res = gatherDepsToUpdate(refsToInstall, info.base, info.channel);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (info.runtime) {
        res = gatherDepsToUpdate(refsToInstall, *info.runtime, info.channel);
        if (!res) {
            return LINGLONG_ERR(res);
        }
    }

    res = gatherExtensionsToUpdate(refsToInstall, info);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

} // namespace linglong::service
