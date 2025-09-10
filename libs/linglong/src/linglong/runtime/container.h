/*
 * SPDX-FileCopyrightText: 2022 - 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Process.hpp"

namespace linglong::runtime {

class Container
{
public:
    Container(ocppi::runtime::config::types::Config cfg,
              std::string appId,
              std::string containerId,
              ocppi::cli::CLI &cli);

    utils::error::Result<void> run(const ocppi::runtime::config::types::Process &process,
                                   ocppi::runtime::RunOption &opt) noexcept;

private:
    ocppi::runtime::config::types::Config cfg;
    std::string id;
    std::string appId;
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime
