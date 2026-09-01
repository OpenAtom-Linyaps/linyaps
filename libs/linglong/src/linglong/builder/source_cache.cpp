// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "source_cache.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace linglong::builder {

SourceCacheLayout::SourceCacheLayout(std::filesystem::path cache)
    : cacheRoot(std::move(cache) / "sources/v1")
    , temporaryRoot(cacheRoot / "tmp")
{
}

auto SourceCacheLayout::object(std::string_view sourceKind, std::string_view hash) const
  -> std::filesystem::path
{
    const auto separator = hash.rfind(':');
    if (separator == std::string_view::npos) {
        return cacheRoot / std::string(sourceKind) / std::string(hash);
    }

    auto algorithm = std::string(hash.substr(0, separator));
    std::replace(algorithm.begin(), algorithm.end(), ':', '-');
    return cacheRoot / std::string(sourceKind) / algorithm
      / std::string(hash.substr(separator + 1));
}

auto SourceCacheLayout::gitObject(std::string_view commit, bool submodules) const
  -> std::filesystem::path
{
    auto normalizedCommit = std::string(commit);
    std::transform(normalizedCommit.begin(),
                   normalizedCommit.end(),
                   normalizedCommit.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });

    return cacheRoot / "git/commit"
      / (normalizedCommit + (submodules ? "-submodules" : "-no-submodules"));
}

auto SourceCacheLayout::lockFile(const std::filesystem::path &entry) const -> std::filesystem::path
{
    auto path = cacheRoot / "locks" / entry.lexically_relative(cacheRoot);
    path += ".lock";
    return path;
}

auto SourceCacheLayout::ensure() noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("ensure source cache directories");

    std::error_code ec;
    auto root = std::filesystem::absolute(cacheRoot, ec);
    if (ec) {
        return LINGLONG_ERR("failed to resolve source cache directory: " + ec.message());
    }

    auto temporary = root / "tmp";
    std::filesystem::create_directories(temporary, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create source cache directories: " + ec.message());
    }

    cacheRoot = std::move(root);
    temporaryRoot = std::move(temporary);

    return LINGLONG_OK;
}

} // namespace linglong::builder
