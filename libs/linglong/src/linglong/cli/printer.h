/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/InspectResult.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::cli {

inline std::string toString(linglong::api::types::v1::SubState subState) noexcept
{
    switch (subState) {
    case linglong::api::types::v1::SubState::AllDone:
        return "AllDone";
    case linglong::api::types::v1::SubState::PackageManagerDone:
        return "PackageManagerDone";
    case linglong::api::types::v1::SubState::InstallApplication:
        return "InstallApplication";
    case linglong::api::types::v1::SubState::InstallBase:
        return "InstallBase";
    case linglong::api::types::v1::SubState::InstallRuntime:
        return "InstallRuntime";
    case linglong::api::types::v1::SubState::PostAction:
        return "PostAction";
    case linglong::api::types::v1::SubState::PreAction:
        return "PreAction";
    case linglong::api::types::v1::SubState::Uninstall:
        return "Uninstall";
    default:
        return "UnknownSubState";
    }
}

inline std::string toString(linglong::api::types::v1::State state) noexcept
{
    switch (state) {
    case linglong::api::types::v1::State::Canceled:
        return "Canceled";
    case linglong::api::types::v1::State::Failed:
        return "Failed";
    case linglong::api::types::v1::State::Processing:
        return "Processing";
    case linglong::api::types::v1::State::Pending:
        return "Pending";
    case linglong::api::types::v1::State::Queued:
        return "Queued";
    case linglong::api::types::v1::State::Succeed:
        return "Succeed";
    default:
        return "UnknownState";
    }
}

class Printer
{
public:
    Printer() = default;
    Printer(const Printer &) = delete;
    Printer(Printer &&) = delete;
    Printer &operator=(const Printer &) = delete;
    Printer &operator=(Printer &&) = delete;
    virtual ~Printer() = default;

    virtual void printErr(const utils::error::Error &) = 0;
    virtual void printPackage(const api::types::v1::PackageInfoV2 &) = 0;
    virtual void printPackages(const std::vector<api::types::v1::PackageInfoV2> &) = 0;
    virtual void printSearchResult(const std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &) = 0;
    virtual void printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &) = 0;
    virtual void printContainers(const std::vector<api::types::v1::CliContainer> &) = 0;
    virtual void printReply(const api::types::v1::CommonResult &) = 0;
    virtual void printRepoConfig(const api::types::v1::RepoConfigV2 &) = 0;
    virtual void printLayerInfo(const api::types::v1::LayerInfo &) = 0;
    virtual void printTaskState(double percentage,
                                const QString &message,
                                api::types::v1::State state,
                                api::types::v1::SubState subState) = 0;
    virtual void printContent(const QStringList &filePaths) = 0;
    virtual void printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &) = 0;
    virtual void printInspect(const api::types::v1::InspectResult &) = 0;
    virtual void printMessage(const QString &message) = 0;
};

} // namespace linglong::cli
