/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
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
    Container(const ocppi::runtime::config::types::Config &cfg,
              const QString &appID,
              const QString &conatinerID,
              ocppi::cli::CLI &cli);

    utils::error::Result<void> run(const ocppi::runtime::config::types::Process &process) noexcept;

private:
    ocppi::runtime::config::types::Config cfg;
    QString id;
    QString appID;
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime
