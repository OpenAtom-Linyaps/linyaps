/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "linglong/runtime/container.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <QDir>

namespace linglong::runtime {

struct ContainerOptions
{
    QString appID;
    QString containerID;

    std::optional<QDir> runtimeDir;       // mount to /runtime
    QDir baseDir;                         // mount to /
    std::optional<QDir> appDir;           // mount to /opt/apps/${info.appid}/files
    std::optional<std::string> appPrefix; // if appPrefix exists, mount appDir to appPrefix

    std::vector<api::types::v1::OciConfigurationPatch> patches;
    std::vector<ocppi::runtime::config::types::Mount> mounts; // extra mounts
    std::vector<std::string> masks;
};

class ContainerBuilder : public QObject
{
    Q_OBJECT
public:
    explicit ContainerBuilder(ocppi::cli::CLI &cli);

    auto create(const ContainerOptions &opts) noexcept
      -> utils::error::Result<QSharedPointer<Container>>;

private:
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime
