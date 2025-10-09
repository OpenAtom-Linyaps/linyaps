/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/package/fallback_version.h"
#include "linglong/utils/error/error.h"

#include <QString>

#include <optional>

namespace linglong::package {
class VersionV2;
class FallbackVersion;

// This is a 4 number semver
class VersionV1 final
{
public:
    static utils::error::Result<VersionV1> parse(const std::string &raw) noexcept;
    explicit VersionV1(const std::string &raw);
    uint64_t major = 0;
    uint64_t minor = 0;
    uint64_t patch = 0;
    std::optional<uint64_t> tweak = {};
    [[nodiscard]] bool semanticMatch(const std::string &versionStr) const noexcept;

    bool operator==(const VersionV1 &that) const noexcept;
    bool operator!=(const VersionV1 &that) const noexcept;
    bool operator<(const VersionV1 &that) const noexcept;
    bool operator>(const VersionV1 &that) const noexcept;
    bool operator<=(const VersionV1 &that) const noexcept;
    bool operator>=(const VersionV1 &that) const noexcept;

    // Compare Version and VersionV2
    friend bool operator==(const VersionV1 &v1, const VersionV2 &v2) noexcept;
    friend bool operator!=(const VersionV1 &v1, const VersionV2 &v2) noexcept;
    friend bool operator<(const VersionV1 &v1, const VersionV2 &v2) noexcept;
    friend bool operator>(const VersionV1 &v1, const VersionV2 &v2) noexcept;
    friend bool operator<=(const VersionV1 &v1, const VersionV2 &v2) noexcept;
    friend bool operator>=(const VersionV1 &v1, const VersionV2 &v2) noexcept;

    // Compare Version and FallbackVersion
    friend bool operator==(const VersionV1 &v1, const FallbackVersion &fv) noexcept;
    friend bool operator!=(const VersionV1 &v1, const FallbackVersion &fv) noexcept;
    friend bool operator<(const VersionV1 &v1, const FallbackVersion &fv) noexcept;
    friend bool operator>(const VersionV1 &v1, const FallbackVersion &fv) noexcept;
    friend bool operator<=(const VersionV1 &v1, const FallbackVersion &fv) noexcept;
    friend bool operator>=(const VersionV1 &v1, const FallbackVersion &fv) noexcept;

    [[nodiscard]] std::string toString() const noexcept;
};
} // namespace linglong::package
