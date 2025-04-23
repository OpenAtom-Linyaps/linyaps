/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/cli/printer.h"

namespace linglong::cli {

class JSONPrinter : public Printer
{
public:
    void printErr(const utils::error::Error &) override;
    void printPackage(const api::types::v1::PackageInfoV2 &) override;
    void printPackages(const std::vector<api::types::v1::PackageInfoV2> &) override;
    void
      printSearchResult(std::map<std::string, std::vector<api::types::v1::PackageInfoV2>>) override;
    void printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &) override;
    void printContainers(const std::vector<api::types::v1::CliContainer> &) override;
    void printReply(const api::types::v1::CommonResult &) override;
    void printRepoConfig(const api::types::v1::RepoConfigV2 &) override;
    void printLayerInfo(const api::types::v1::LayerInfo &) override;
    void printTaskState(double percentage,
                        const QString &message,
                        api::types::v1::State state,
                        api::types::v1::SubState subState) override;
    void printContent(const QStringList &desktopPaths) override;
    void printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &) override;
    void printInspect(const api::types::v1::InspectResult &) override;
    void printMessage([[maybe_unused]] const QString &message) override { };
};

} // namespace linglong::cli
