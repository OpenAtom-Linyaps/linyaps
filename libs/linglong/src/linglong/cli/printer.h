/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::cli {

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
    virtual void printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &) = 0;
    virtual void printContainers(const std::vector<api::types::v1::CliContainer> &) = 0;
    virtual void printReply(const api::types::v1::CommonResult &) = 0;
    virtual void printRepoConfig(const api::types::v1::RepoConfig &) = 0;
    virtual void printLayerInfo(const api::types::v1::LayerInfo &) = 0;
    virtual void printTaskState(double percentage,
                                const QString &message,
                                api::types::v1::State state,
                                api::types::v1::SubState subState) = 0;
    virtual void printContent(const QStringList &filePaths) = 0;
    virtual void printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &) = 0;
};

} // namespace linglong::cli
