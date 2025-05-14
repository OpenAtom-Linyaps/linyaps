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
    static utils::error::Result<VersionV1> parse(const QString &raw) noexcept;
    explicit VersionV1(const QString &raw);
    qlonglong major = 0;
    qlonglong minor = 0;
    qlonglong patch = 0;
    std::optional<qlonglong> tweak = {};
    bool semanticMatch(const QString &versionStr) const noexcept;

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

    QString toString() const noexcept;
};
} // namespace linglong::package
