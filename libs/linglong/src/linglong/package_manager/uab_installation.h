// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/InteractionMessageType.hpp"
#include "linglong/api/types/v1/PackageManager1RequestInteractionAdditionalMessage.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/package/uab_file.h"
#include "linglong/package_manager/action.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/transaction.h"

#include <optional>
#include <vector>

namespace linglong::service {

class UabInstallationAction : public Action
{
public:
    static std::shared_ptr<UabInstallationAction> create(int uabFD,
                                                         PackageManager &pm,
                                                         repo::OSTreeRepo &repo,
                                                         api::types::v1::CommonOptions options);

    using CheckedLayers = std::pair<std::vector<linglong::api::types::v1::UabLayer>,
                                    std::vector<linglong::api::types::v1::UabLayer>>;
    static utils::error::Result<CheckedLayers> checkExecModeUABLayers(
      repo::OSTreeRepo &repo, const std::vector<linglong::api::types::v1::UabLayer> &layers);
    static utils::error::Result<CheckedLayers> checkDistributionModeUABLayers(
      repo::OSTreeRepo &repo, const std::vector<linglong::api::types::v1::UabLayer> &layers);
    static utils::error::Result<void> checkUABLayersConstrain(
      repo::OSTreeRepo &repo, const std::vector<api::types::v1::UabLayer> &layers);
    static bool extraModuleOnly(const std::vector<api::types::v1::UabLayer> &layers);

    virtual ~UabInstallationAction();

    virtual utils::error::Result<void> prepare();
    virtual utils::error::Result<void> doAction(PackageTask &task);

    virtual std::string getTaskName() const { return taskName; }

protected:
    virtual utils::error::Result<void> preInstall(PackageTask &task);
    virtual utils::error::Result<void> install(PackageTask &task);
    virtual utils::error::Result<void> postInstall(PackageTask &task);

private:
    UabInstallationAction(int uabFD,
                          PackageManager &pm,
                          repo::OSTreeRepo &repo,
                          api::types::v1::CommonOptions options);

    utils::error::Result<void> installExecModeUAB(PackageTask &task);
    utils::error::Result<void> installDistributionModeUAB(PackageTask &task);
    utils::error::Result<void> installUabLayer(const std::vector<api::types::v1::UabLayer> &layers,
                                               std::optional<std::string> subRef = std::nullopt);

    int fd;
    ActionOperation operation;
    std::string taskName;
    CheckedLayers checkedLayers;
    std::unique_ptr<package::UABFile> uabFile;
    utils::Transaction transaction;
    std::filesystem::path uabMountPoint;
};

} // namespace linglong::service
