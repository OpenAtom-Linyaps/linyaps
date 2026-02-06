// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/package/reference.h"
#include "linglong/package_manager/action.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/transaction.h"

namespace linglong::package {
class FuzzyReference;
}

namespace linglong::service {

class Task;

class PackageUpdateAction : public Action
{
public:
    using RefsToInstall =
      std::vector<std::pair<package::ReferenceWithRepo,
                            std::vector<std::pair<std::string, repo::RefMetaData>>>>;

    static std::shared_ptr<PackageUpdateAction>
    create(std::vector<api::types::v1::PackageManager1Package> toUpgrade,
           bool appOnly,
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
                        bool appOnly,
                        bool depsOnly,
                        PackageManager &pm,
                        repo::OSTreeRepo &repo);

    utils::error::Result<void>
    updateApp(Task &task, const api::types::v1::PackageInfoV2 &app, bool appOnly, bool depsOnly);
    utils::error::Result<void> postUpdateApp(Task &task,
                                             const package::Reference &localRef,
                                             const package::ReferenceWithRepo &remoteRef);

    utils::error::Result<void> gatherRefsToUpdate(RefsToInstall &refsToInstall,
                                                  const package::FuzzyReference &fuzzyRef,
                                                  std::optional<package::Reference> &local,
                                                  bool installIfMissing = false);
    utils::error::Result<void> gatherExtensionsToUpdate(RefsToInstall &refsToInstall,
                                                        const api::types::v1::PackageInfoV2 &info);
    utils::error::Result<void> gatherDepsToUpdate(RefsToInstall &refsToInstall,
                                                  const std::string &refStr,
                                                  const std::string &channel,
                                                  bool isExtension = false);
    utils::error::Result<void> gatherAppDepsToUpgrade(RefsToInstall &refsToInstall,
                                                      const api::types::v1::PackageInfoV2 &info);

    std::vector<api::types::v1::PackageManager1Package> toUpgrade;
    bool appOnly;
    bool depsOnly;

    std::string taskName;
    std::string taskMessage;
    utils::Transaction transaction;
    bool prepared = false;
    std::vector<api::types::v1::PackageInfoV2> appsToUpgrade;
    uint64_t taskTotalSize;
    uint64_t taskNeededSize;
    uint64_t taskFetchedSize;
};

} // namespace linglong::service
