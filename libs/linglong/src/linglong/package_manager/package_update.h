// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/package/reference.h"
#include "linglong/package_manager/action.h"
#include "linglong/utils/transaction.h"

namespace linglong::package {
class FuzzyReference;
}

namespace linglong::service {

class Task;

class PackageUpdateAction : public Action
{
public:
    static std::shared_ptr<PackageUpdateAction>
    create(std::vector<api::types::v1::PackageManager1Package> toUpgrade,
           bool depsOnly,
           PackageManager &pm,
           repo::OSTreeRepo &repo);

    virtual ~PackageUpdateAction() = default;

    virtual utils::error::Result<void> prepare() override;
    virtual utils::error::Result<void> doAction(PackageTask &task) override;

    virtual std::string getTaskName() const override { return taskName; }

protected:
    virtual utils::error::Result<void> update(PackageTask &task);
    virtual utils::error::Result<void> postUpdate(Task &task);

private:
    PackageUpdateAction(std::vector<api::types::v1::PackageManager1Package> toUpgrade,
                        bool depsOnly,
                        PackageManager &pm,
                        repo::OSTreeRepo &repo);

    utils::error::Result<void> updateApp(Task &task,
                                         const api::types::v1::PackageInfoV2 &app,
                                         bool depsOnly);
    utils::error::Result<void> updateApp(Task &task,
                                         const package::Reference &localRef,
                                         const package::ReferenceWithRepo &remoteRef);
    utils::error::Result<void> updateRef(Task &task,
                                         const package::Reference &local,
                                         const package::ReferenceWithRepo &remote);
    utils::error::Result<void> updateAppDepends(Task &task,
                                                const api::types::v1::PackageInfoV2 &app);
    void updateExtensions(Task &task, const api::types::v1::PackageInfoV2 &info);
    utils::error::Result<void> updateDependsRef(Task &task,
                                                const std::string &refStr,
                                                std::optional<std::string> channel = std::nullopt,
                                                std::optional<std::string> version = std::nullopt,
                                                bool isExtension = false);
    utils::error::Result<void> postUpdateApp(Task &task,
                                             const package::Reference &localRef,
                                             const package::ReferenceWithRepo &remoteRef);

    utils::error::Result<std::reference_wrapper<package::ReferenceWithRepo>>
    latestRemoteReference(const package::FuzzyReference &fuzzyRef);

    std::vector<api::types::v1::PackageManager1Package> toUpgrade;
    bool depsOnly;

    std::string taskName;
    utils::Transaction transaction;
    bool prepared = false;
    std::vector<api::types::v1::PackageInfoV2> appsToUpgrade;
    std::map<std::string, package::ReferenceWithRepo> candidates;
};

} // namespace linglong::service
