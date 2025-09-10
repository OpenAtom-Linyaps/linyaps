/*
 * SPDX-FileCopyrightText: 2022 - 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

namespace linglong::runtime {

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

auto ContainerBuilder::create(const linglong::generator::ContainerCfgBuilder &cfgBuilder) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create container");
    auto config = cfgBuilder.getConfig();

    return std::make_unique<Container>(config,
                                       cfgBuilder.getAppId(),
                                       cfgBuilder.getContainerId(),
                                       this->cli);
}

} // namespace linglong::runtime
