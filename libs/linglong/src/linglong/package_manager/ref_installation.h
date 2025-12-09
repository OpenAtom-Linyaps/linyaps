// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/package/fuzzy_reference.h"
#include "linglong/package_manager/action.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/utils/transaction.h"

namespace linglong::service {

class Task;

class RefInstallationAction : public Action
{
public:
    static std::shared_ptr<RefInstallationAction>
    create(package::FuzzyReference fuzzyRef,
           std::vector<std::string> modules,
           PackageManager &pm,
           repo::OSTreeRepo &repo,
           api::types::v1::CommonOptions options,
           std::optional<api::types::v1::Repo> usedRepo);

    static void checkModules(const std::vector<std::string> &modules,
                             bool &hasBinary,
                             bool &extraModule);
    static bool extraModuleOnly(const std::vector<std::string> &modules);

    virtual ~RefInstallationAction() = default;

    utils::error::Result<void> prepare() override { return LINGLONG_OK; }

    utils::error::Result<void> doAction(PackageTask &task) override;

    std::string getTaskName() const override { return taskName; }

private:
    utils::error::Result<void> preInstall(Task &task);
    utils::error::Result<void> install(Task &task);
    utils::error::Result<void> postInstall(Task &task);

    RefInstallationAction(package::FuzzyReference fuzzyRef,
                          std::vector<std::string> modules,
                          PackageManager &pm,
                          repo::OSTreeRepo &repo,
                          api::types::v1::CommonOptions options,
                          std::optional<api::types::v1::Repo> usedRepo);

    utils::error::Result<void> postInstallApp(Task &task);

    package::FuzzyReference fuzzyRef;
    std::vector<std::string> modules;

    ActionOperation operation;
    std::string taskName;
    utils::Transaction transaction;
    std::optional<api::types::v1::Repo> usedRepo;
    repo::RemotePackages candidates;
    PackageTask *mainTask;
};

} // namespace linglong::service
