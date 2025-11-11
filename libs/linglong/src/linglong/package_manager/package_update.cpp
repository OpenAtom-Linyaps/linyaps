// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_update.h"

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/extension/extension.h"
#include "linglong/package_manager/package_manager.h"
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
    taskName = fmt::format("update {} apps", appsToUpgrade.size());
    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::doAction(PackageTask &task)
{
    LINGLONG_TRACE("package update action");

    task.updateSubState(linglong::api::types::v1::SubState::PreAction, "prepared");

    if (!prepared) {
        return LINGLONG_ERR("action not prepared");
    }

    auto res = update(task);
    if (!res) {
        return LINGLONG_ERR(res.error());
    }

    return LINGLONG_OK;
}

utils::error::Result<void> PackageUpdateAction::update(PackageTask &task)
{
    LINGLONG_TRACE("package update");

    task.updateState(linglong::api::types::v1::State::Processing, "update apps");

    bool allFailed = true;
    for (const auto &app : appsToUpgrade) {
        auto res = updateApp(task, app, depsOnly);
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

utils::error::Result<void> PackageUpdateAction::updateApp(PackageTask &task,
                                                          const api::types::v1::PackageInfoV2 &app,
                                                          bool depsOnly)
{
    LINGLONG_TRACE(fmt::format("update app: {} dpesOnly: {}", app.id, depsOnly));

    if (depsOnly) {
        return updateAppDepends(task, app);
    }

    task.updateSubState(linglong::api::types::v1::SubState::InstallApplication,
                        fmt::format("updating application {}", app.id));

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

    if (remoteRef->get().reference.version <= localRef->version) {
        return updateAppDepends(task, app);
    }

    return updateApp(task, *localRef, remoteRef->get());
}

utils::error::Result<void>
PackageUpdateAction::updateApp(PackageTask &task,
                               const package::Reference &localRef,
                               const package::ReferenceWithRepo &remoteRef)
{
    LINGLONG_TRACE(
      fmt::format("update app from {} to {}", localRef.toString(), remoteRef.reference.toString()));

    auto res = updateRef(task, localRef, remoteRef);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    // use updated package info
    auto app = repo.getLayerItem(remoteRef.reference);
    if (!app) {
        return LINGLONG_ERR(app.error());
    }

    res = updateAppDepends(task, app->info);
    if (!res) {
        return LINGLONG_ERR(res.error());
    }

    return postUpdateApp(task, localRef, remoteRef);
}

utils::error::Result<void> PackageUpdateAction::updateRef(PackageTask &task,
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
PackageUpdateAction::updateAppDepends(PackageTask &task, const api::types::v1::PackageInfoV2 &app)
{
    LINGLONG_TRACE(fmt::format("update app depends for {}", app.id));

    task.updateSubState(linglong::api::types::v1::SubState::InstallBase, "updating base");
    auto ret = updateDependsRef(task, app.base, app.channel);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (app.runtime) {
        task.updateSubState(linglong::api::types::v1::SubState::InstallRuntime, "updating runtime");
        ret = updateDependsRef(task, *app.runtime, app.channel);
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
    }

    updateExtensions(task, app);

    return LINGLONG_OK;
}

void PackageUpdateAction::updateExtensions(PackageTask &task,
                                           const api::types::v1::PackageInfoV2 &info)
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

utils::error::Result<void> PackageUpdateAction::updateDependsRef(PackageTask &task,
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
PackageUpdateAction::postUpdateApp([[maybe_unused]] PackageTask &task,
                                   const package::Reference &localRef,
                                   const package::ReferenceWithRepo &remoteRef)
{
    LINGLONG_TRACE("post update app");

    task.updateSubState(linglong::api::types::v1::SubState::PostAction, "post update app");

    auto res = pm.tryGenerateCache(remoteRef.reference);
    if (!res) {
        return LINGLONG_ERR(res.error());
    }

    res = pm.tryUninstallRef(localRef);
    if (!res) {
        LogW("failed to try uninstall ref:{}", res.error());
    }

    auto mergeRes = repo.mergeModules();
    if (!mergeRes) {
        LogE("failed to merge modules: {}", mergeRes.error());
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
