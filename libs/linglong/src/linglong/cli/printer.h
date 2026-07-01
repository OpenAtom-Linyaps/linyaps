/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/InspectResult.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/PackageInfoDisplay.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/cli/cli.h"
#include "linglong/utils/error/error.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace linglong::cli {

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
    struct ModuleSizeInfo
    {
        std::string id;
        std::string name;
        std::string version;
        std::string channel;
        std::string module;
        std::uint64_t exclusiveSize{ 0 };
        std::uint64_t sharedSize{ 0 };
        std::uint64_t logicalSize{ 0 };
        std::uint64_t actualSize{ 0 };
    };

    struct DependsNode
    {
        std::string ref;
        std::string kind;
        std::vector<DependsNode> children;
    };

    Printer() = default;
    Printer(const Printer &) = delete;
    Printer(Printer &&) = delete;
    Printer &operator=(const Printer &) = delete;
    Printer &operator=(Printer &&) = delete;
    virtual ~Printer() = default;

    virtual void printErr(const utils::error::Error &) = 0;
    virtual void printPackage(const api::types::v1::PackageInfoV2 &) = 0;
    virtual void printPackages(const std::vector<api::types::v1::PackageInfoDisplay> &) = 0;
    virtual void
      printSearchResult(std::map<std::string, std::vector<api::types::v1::PackageInfoV2>>) = 0;
    virtual void printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &) = 0;
    virtual void printContainers(const std::vector<api::types::v1::CliContainer> &) = 0;
    virtual void printRepoConfig(const api::types::v1::RepoConfigV2 &) = 0;
    virtual void printLayerInfo(const api::types::v1::LayerInfo &) = 0;
    virtual void printProgress(double percentage, const std::string &message) = 0;
    virtual void printContent(const QStringList &filePaths) = 0;
    virtual void printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &) = 0;
    virtual void printInspect(const api::types::v1::InspectResult &) = 0;
    virtual void printModuleSizes(const std::vector<ModuleSizeInfo> &list,
                                  std::uint64_t actualTotalSize,
                                  std::uint64_t repoSize) = 0;
    virtual void printDepends(const std::vector<DependsNode> &trees) = 0;
    virtual void printMessage(const std::string &message) = 0;

    virtual void clearLine() { }
};

} // namespace linglong::cli
