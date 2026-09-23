/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/utils/log/log.h"
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

utils::error::Result<api::types::v1::PackageInfoV2>
LayerDir::readInfoIfMatches(const api::types::v1::PackageInfoV2 &expected) const
{
    auto actual = info();
    if (!actual) {
        return LINGLONG_ERR(actual);
    }

    if (nlohmann::json(*actual) != nlohmann::json(expected)) {
        return LINGLONG_ERR(
          fmt::format("layer metadata does not match info.json in {}", this->path_.string()));
    }

    return actual;
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

TempLayerDir::TempLayerDir(TempLayerDir &&other) noexcept
    : layerDir_(std::move(other.layerDir_))
    , ownsPath_(other.ownsPath_)
{
    other.ownsPath_ = false;
}

TempLayerDir &TempLayerDir::operator=(TempLayerDir &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    remove();
    layerDir_ = std::move(other.layerDir_);
    ownsPath_ = other.ownsPath_;
    other.ownsPath_ = false;
    return *this;
}

TempLayerDir::~TempLayerDir() noexcept
{
    remove();
}

void TempLayerDir::remove() noexcept
{
    if (!ownsPath_ || layerDir_.path().empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove_all(layerDir_.path(), ec);
    if (ec) {
        LogW("failed to remove temporary layer directory {}: {}", layerDir_.path(), ec.message());
    }
    ownsPath_ = false;
}

} // namespace linglong::package
