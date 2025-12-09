// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_update.h"

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/extension/extension.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/log/log.h"

namespace linglong::service {

std::shared_ptr<PackageUpdateAction>
PackageUpdateAction::create(std::vector<api::types::v1::PackageManager1Package> toUpgrade,
                            bool depsOnly,
                            PackageManager &pm,
                            repo::OSTreeRepo &repo)
{
    auto p = new PackageUpdateAction(std::move(toUpgrade), depsOnly, pm, repo);
    return std::shared_ptr<PackageUpdateAction>(p);
}

PackageUpdateAction::PackageUpdateAction(
  std::vector<api::types::v1::PackageManager1Package> toUpgrade,
  bool depsOnly,
  PackageManager &pm,
  repo::OSTreeRepo &repo)
    : Action(pm, repo, api::types::v1::CommonOptions{})
    , toUpgrade(std::move(toUpgrade))
    , depsOnly(depsOnly)
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

    task.updateState(linglong::api::types::v1::State::Processing, "updating applications");

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

    TaskContainer container(task, appsToUpgrade.size());
    bool allFailed = true;
    for (const auto &app : appsToUpgrade) {
        if (task.isTaskDone()) {
            return LINGLONG_ERR("task was cancelled");
        }

        auto res = updateApp(container.next(), app, depsOnly);
        if (!res) {
            LogW("failed to update app {}: {}", app.id, res.error());
            continue;
        }
        allFailed = false;
    }

    if (allFailed) {
        return LINGLONG_ERR("all apps failed to upgrade",
                            utils::error::ErrorCode::AppUpgradeFailed);
    }

    task.updateState(linglong::api::types::v1::State::Succeed, "update apps success");

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
                                                          bool depsOnly)
{
    LINGLONG_TRACE(fmt::format("update app: {} dpesOnly: {}", app.id, depsOnly));

    task.updateProgress(1, fmt::format("updating {}", app.id));

    if (depsOnly) {
        return updateAppDepends(task, app);
    }

    auto localRef = package::Reference::fromPackageInfo(app);
    if (!localRef) {
        return LINGLONG_ERR(localRef.error());
    }

    auto fuzzyRef =
      package::FuzzyReference::create(app.channel, app.id, std::nullopt, std::nullopt);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef.error());
    }

    auto remoteRef = this->latestRemoteReference(*fuzzyRef);
    if (!remoteRef) {
        return LINGLONG_ERR(remoteRef.error());
    }

    task.updateProgress(5);

    if (remoteRef->get().reference.version <= localRef->version) {
        return updateAppDepends(task, app);
    }

    return updateApp(task, *localRef, remoteRef->get());
}

utils::error::Result<void> PackageUpdateAction::updateApp(
  Task &task, const package::Reference &localRef, const package::ReferenceWithRepo &remoteRef)
{
    LINGLONG_TRACE(
      fmt::format("update app from {} to {}", localRef.toString(), remoteRef.reference.toString()));

    TaskContainer container(task, 3);
    auto res = updateRef(container.next(), localRef, remoteRef);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    // use updated package info
    auto app = repo.getLayerItem(remoteRef.reference);
    if (!app) {
        return LINGLONG_ERR(app.error());
    }

    res = updateAppDepends(container.next(), app->info);
    if (!res) {
        return LINGLONG_ERR(res.error());
    }

    return postUpdateApp(container.next(), localRef, remoteRef);
}

utils::error::Result<void> PackageUpdateAction::updateRef(Task &task,
                                                          const package::Reference &local,
                                                          const package::ReferenceWithRepo &remote)
{
    LINGLONG_TRACE(
      fmt::format("update ref from {} to {}", local.toString(), remote.reference.toString()));

    auto modules = repo.getModuleList(local);
    auto remoteModules = repo.getRemoteModuleList(remote.reference, remote.repo);
    if (!remoteModules) {
        return LINGLONG_ERR(remoteModules.error());
    }

    if (remoteModules->empty()) {
        return LINGLONG_ERR(fmt::format("no modules found for {}", remote.reference.toString()),
                            utils::error::ErrorCode::AppUpgradeFailed);
    }

    task.updateProgress(5);

    auto installModules = std::vector<std::string>{};
    for (const auto &module : modules) {
        if (std::find(remoteModules->begin(), remoteModules->end(), module)
            != remoteModules->end()) {
            installModules.emplace_back(module);
            continue;
        }

        // update to binary module if runtime module is not found
        if (module == "runtime"
            && std::find(remoteModules->begin(), remoteModules->end(), "binary")
              != remoteModules->end()) {
            installModules.emplace_back("binary");
            continue;
        }
    }
    if (installModules.empty()) {
        return LINGLONG_ERR(fmt::format("no modules found to upgrade {}", local.toString()),
                            utils::error::ErrorCode::AppUpgradeFailed);
    }

    return pm.installRef(task, remote, installModules);
}

utils::error::Result<void>
PackageUpdateAction::updateAppDepends(Task &task, const api::types::v1::PackageInfoV2 &app)
{
    LINGLONG_TRACE(fmt::format("update app depends for {}", app.id));

    TaskContainer container(task, 3);

    auto ret = updateDependsRef(container.next(), app.base, app.channel);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (app.runtime) {
        ret = updateDependsRef(container.next(), *app.runtime, app.channel);
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
    }

    updateExtensions(container.next(), app);

    return LINGLONG_OK;
}

void PackageUpdateAction::updateExtensions(Task &task, const api::types::v1::PackageInfoV2 &info)
{
    if (!info.extensions) {
        return;
    }

    for (const auto &extension : *info.extensions) {
        std::string name = extension.name;
        auto ext = extension::ExtensionFactory::makeExtension(name);
        if (!ext->shouldEnable(name)) {
            continue;
        }
        auto ret = updateDependsRef(task, name, info.channel, extension.version, true);
        if (!ret) {
            LogW("failed to update extension {}", name);
            continue;
        }
    }
}

utils::error::Result<void> PackageUpdateAction::updateDependsRef(Task &task,
                                                                 const std::string &refStr,
                                                                 std::optional<std::string> channel,
                                                                 std::optional<std::string> version,
                                                                 bool isExtension)
{
    LINGLONG_TRACE(fmt::format("update depends ref {}", refStr));

    auto fuzzyRef = package::FuzzyReference::parse(refStr);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef.error());
    }

    // use provided channel/version if not set in fuzzyRef
    if (channel && !fuzzyRef->channel) {
        fuzzyRef->channel = *channel;
    }
    if (version && !fuzzyRef->version) {
        fuzzyRef->version = version;
    }

    auto local = this->repo.clearReference(*fuzzyRef,
                                           {
                                             .forceRemote = false,
                                             .fallbackToRemote = false,
                                             .semanticMatching = true,
                                           });

    std::optional<std::reference_wrapper<package::Reference>> ref;
    if (local) {
        auto remote = this->latestRemoteReference(*fuzzyRef);
        if (remote && remote->get().reference.version > local->version) {
            auto res = updateRef(task, *local, *remote);
            if (!res) {
                return LINGLONG_ERR(res.error());
            }
            ref = remote->get().reference;
        } else {
            ref = *local;
        }

        // try update extensions if this is not an extension
        if (!isExtension) {
            auto current = repo.getLayerItem(*ref);
            if (!current) {
                return LINGLONG_ERR(current.error());
            }
            updateExtensions(task, current->info);
        }
    } else {
        // TODO auto-install
    }

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

utils::error::Result<std::reference_wrapper<package::ReferenceWithRepo>>
PackageUpdateAction::latestRemoteReference(const package::FuzzyReference &fuzzyRef)
{
    LINGLONG_TRACE("latest remote reference from cache");

    auto key = fuzzyRef.toString();
    if (candidates.find(key) != candidates.end()) {
        return candidates.at(key);
    }

    auto remote = repo.latestRemoteReference(fuzzyRef);
    if (!remote) {
        return LINGLONG_ERR(remote.error());
    }
    auto res = candidates.emplace(key, std::move(remote).value());
    return res.first->second;
}

} // namespace linglong::service
