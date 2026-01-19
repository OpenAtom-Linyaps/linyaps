/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>

namespace linglong::package {

class LayerDir
{
public:
    LayerDir(std::filesystem::path path)
        : path_(std::move(path))
    {
    }

    [[nodiscard]] utils::error::Result<api::types::v1::PackageInfoV2> info() const;
    [[nodiscard]] std::filesystem::path filesDirPath() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] std::filesystem::path path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace linglong::package
