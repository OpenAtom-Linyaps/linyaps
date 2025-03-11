/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/package/fallback_version.h"
#include "linglong/package/versionv2.h"
#include "linglong/utils/error/error.h"

#include <QString>

#include <optional>
#include <variant>

namespace linglong::package {
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

    bool operator==(const VersionV1 &that) const noexcept;
    bool operator!=(const VersionV1 &that) const noexcept;
    bool operator<(const VersionV1 &that) const noexcept;
    bool operator>(const VersionV1 &that) const noexcept;
    bool operator<=(const VersionV1 &that) const noexcept;
    bool operator>=(const VersionV1 &that) const noexcept;

    // Compare Version and VersionV2
    bool operator==(const VersionV2 &that) const noexcept;
    bool operator!=(const VersionV2 &that) const noexcept;
    bool operator<(const VersionV2 &that) const noexcept;
    bool operator>(const VersionV2 &that) const noexcept;
    bool operator<=(const VersionV2 &that) const noexcept;
    bool operator>=(const VersionV2 &that) const noexcept;

    // Compare Version and FallbackVersion
    bool operator==(const FallbackVersion &that) const noexcept;
    bool operator!=(const FallbackVersion &that) const noexcept;
    bool operator<(const FallbackVersion &that) const noexcept;
    bool operator>(const FallbackVersion &that) const noexcept;
    bool operator<=(const FallbackVersion &that) const noexcept;
    bool operator>=(const FallbackVersion &that) const noexcept;

    QString toString() const noexcept;
};

class Version final
{
public:
    static utils::error::Result<Version> parse(const QString &raw,
                                               const bool &fallback = true) noexcept;
    explicit Version(const QString &raw) = delete;

    explicit Version(const VersionV1 &version) { this->version = version; };

    explicit Version(const VersionV2 &version) { this->version = version; };

    explicit Version(const FallbackVersion &version) { this->version = version; };

    void ignoreTweak() noexcept;
    bool isVersionV1() noexcept;
    bool hasTweak() noexcept;

    bool operator==(const Version &that) const noexcept;
    bool operator!=(const Version &that) const noexcept;
    bool operator<(const Version &that) const noexcept;
    bool operator>(const Version &that) const noexcept;
    bool operator<=(const Version &that) const noexcept;
    bool operator>=(const Version &that) const noexcept;

    QString toString() const noexcept;

private:
    std::variant<VersionV2, VersionV1, FallbackVersion> version;
};

} // namespace linglong::package
