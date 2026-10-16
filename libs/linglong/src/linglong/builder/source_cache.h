// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string_view>

namespace linglong::builder {

class SourceCacheLayout
{
public:
    explicit SourceCacheLayout(std::filesystem::path cacheRoot);

    const std::filesystem::path &root() const noexcept { return cacheRoot; }

    const std::filesystem::path &temporary() const noexcept { return temporaryRoot; }

    auto object(std::string_view sourceKind, std::string_view hash) const -> std::filesystem::path;

    auto gitObject(std::string_view commit, bool submodules) const -> std::filesystem::path;

    // Lock files remain in place when their cache entries are replaced.
    auto lockFile(const std::filesystem::path &entry) const -> std::filesystem::path;

    auto ensure() noexcept -> utils::error::Result<void>;

private:
    std::filesystem::path cacheRoot;
    std::filesystem::path temporaryRoot;
};

} // namespace linglong::builder
