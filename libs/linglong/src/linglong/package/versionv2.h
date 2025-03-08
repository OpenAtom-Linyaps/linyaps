/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

namespace linglong::package {
class Version;

class VersionV2 final
{
public:
    static utils::error::Result<VersionV2> parse(const QString &raw) noexcept;
    explicit VersionV2(uint64_t major = 0,
                       uint64_t minor = 0,
                       uint64_t patch = 0,
                       const std::string &prerelease = "",
                       std::string build_meta = "",
                       uint64_t security = 0);
    qlonglong major = 0;
    qlonglong minor = 0;
    qlonglong patch = 0;
    std::string prerelease;
    std::string buildMeta;
    qlonglong security = 0;

    bool operator==(const VersionV2 &that) const noexcept;
    bool operator!=(const VersionV2 &that) const noexcept;
    bool operator<(const VersionV2 &that) const noexcept;
    bool operator>(const VersionV2 &that) const noexcept;
    bool operator<=(const VersionV2 &that) const noexcept;
    bool operator>=(const VersionV2 &that) const noexcept;

    // 比较Version 和 VersionV2
    bool operator==(const linglong::package::Version &that) const noexcept;
    bool operator!=(const linglong::package::Version &that) const noexcept;
    bool operator<(const linglong::package::Version &that) const noexcept;
    bool operator>(const linglong::package::Version &that) const noexcept;
    bool operator<=(const linglong::package::Version &that) const noexcept;
    bool operator>=(const linglong::package::Version &that) const noexcept;

    QString toString() const noexcept;
};
} // namespace linglong::package
