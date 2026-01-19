/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/utils/serialize/packageinfo_handler.h"

#include <fmt/format.h>

namespace linglong::package {

utils::error::Result<api::types::v1::PackageInfoV2> LayerDir::info() const
{
    LINGLONG_TRACE(fmt::format("get layer info from {}", this->path_.string()));

    auto info = utils::serialize::parsePackageInfoFile(this->path_ / "info.json");
    if (!info) {
        return LINGLONG_ERR(info);
    }

    return info;
}

std::filesystem::path LayerDir::filesDirPath() const noexcept
{
    return this->path_ / "files";
}

bool LayerDir::valid() const noexcept
{
    std::error_code ec;
    return std::filesystem::exists(this->path_ / "info.json", ec);
}

} // namespace linglong::package
