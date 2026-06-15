/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/utils/error/error.h"

#include <CLI/CLI.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace linglong::common::cli {

struct RepoOptions
{
    std::string repoName;
    std::string repoUrl;
    std::optional<std::string> repoAlias;
    std::int64_t repoPriority{ 0 };
};

struct RepoConfigBackend
{
    std::function<utils::error::Result<api::types::v1::RepoConfigV2>()> getConfig;
    std::function<utils::error::Result<void>(const api::types::v1::RepoConfigV2 &)> setConfig;
};

struct RepoCommandCallbacks
{
    std::function<void(const api::types::v1::RepoConfigV2 &)> showConfig;
};

CLI::App *addRepoCommand(CLI::App &commandParser,
                         RepoOptions &repoOptions,
                         const std::string &group,
                         const CLI::Validator &validatorString,
                         const std::string &programName);

utils::error::Result<void> handleRepoCommand(CLI::App *app,
                                             const RepoOptions &options,
                                             const RepoConfigBackend &backend,
                                             const RepoCommandCallbacks &callbacks);

} // namespace linglong::common::cli
