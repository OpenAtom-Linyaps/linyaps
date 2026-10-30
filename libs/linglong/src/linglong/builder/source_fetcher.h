/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderProjectSource.hpp"
#include "linglong/api/types/v1/BuilderSourceLockResolved.hpp"
#include "linglong/builder/source_cache.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace linglong::builder {

struct GenerateSourceRequest
{
    // Acquire fresh content and validate the manifest digest before publishing it to the cache.
    api::types::v1::BuilderProjectSource source;
};

struct LockedSourceRequest
{
    // Reuse verified cached content when available, otherwise acquire the locked content.
    api::types::v1::BuilderProjectSource source;
    api::types::v1::BuilderSourceLockResolved resolved;
};

using SourceFetchRequest = std::variant<GenerateSourceRequest, LockedSourceRequest>;

auto fileHash(const std::filesystem::path &file) noexcept -> utils::error::Result<std::string>;

auto fileNameFromUrl(std::string_view url) -> std::string;

auto sourceOutputName(const api::types::v1::BuilderProjectSource &source) -> std::string;

auto gitCommitFromHash(std::string_view hash) -> std::optional<std::string>;

class SourceFetcher
{
public:
    explicit SourceFetcher(SourceFetchRequest request, SourceCacheLayout cache);

    // The caller must ensure the source cache layout and destination directory exist.
    auto fetch(const std::filesystem::path &destination, bool offline = false) noexcept
      -> utils::error::Result<api::types::v1::BuilderSourceLockResolved>;

private:
    SourceCacheLayout cache;
    SourceFetchRequest request;
};

} // namespace linglong::builder
