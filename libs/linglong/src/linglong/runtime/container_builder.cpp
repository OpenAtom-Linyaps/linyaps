/*
 * SPDX-FileCopyrightText: 2022 - 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/common/dir.h"
#include "linglong/utils/log/log.h"

namespace linglong::runtime {

utils::error::Result<std::filesystem::path> makeBundleDir(const std::string &containerID,
                                                          const std::string &bundleSuffix)
{
    LINGLONG_TRACE("get bundle dir");
    auto bundle = common::dir::getBundleDir(containerID);
    if (!bundleSuffix.empty()) {
        bundle += bundleSuffix;
    }
    std::error_code ec;
    if (std::filesystem::exists(bundle, ec)) {
        std::filesystem::remove_all(bundle, ec);
        if (ec) {
            LogW("failed to remove bundle directory {}: {}", bundle.c_str(), ec.message());
        }
    }

    if (!std::filesystem::create_directories(bundle, ec) && ec) {
        return LINGLONG_ERR(fmt::format("failed to create bundle directory {}", bundle), ec);
    }

    return bundle;
}

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

auto ContainerBuilder::create(const linglong::generator::ContainerCfgBuilder &cfgBuilder) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    auto config = cfgBuilder.getConfig();

    return std::make_unique<Container>(config,
                                       cfgBuilder.getContainerId(),
                                       cfgBuilder.getBundlePath(),
                                       this->cli);
}

} // namespace linglong::runtime
