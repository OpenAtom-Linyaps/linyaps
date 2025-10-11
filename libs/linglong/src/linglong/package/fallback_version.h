/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "linglong/utils/error/error.h"

#include <QStringList>

namespace linglong::package {
class VersionV2;
class VersionV1;

class FallbackVersion
{
public:
    static utils::error::Result<FallbackVersion> parse(const std::string &raw) noexcept;

    explicit FallbackVersion(const std::vector<std::string> &list)
        : list(list) { };

    [[nodiscard]] bool semanticMatch(const std::string &versionStr) const noexcept;

    [[nodiscard]] std::string toString() const noexcept;

    bool operator==(const FallbackVersion &that) const;
    bool operator!=(const FallbackVersion &that) const;
    bool operator>(const FallbackVersion &that) const;
    bool operator<(const FallbackVersion &that) const;
    bool operator>=(const FallbackVersion &that) const;
    bool operator<=(const FallbackVersion &that) const;

    friend bool operator==(const FallbackVersion &fv, const VersionV1 &v1);
    friend bool operator!=(const FallbackVersion &fv, const VersionV1 &v1);
    friend bool operator>(const FallbackVersion &fv, const VersionV1 &v1);
    friend bool operator<(const FallbackVersion &fv, const VersionV1 &v1);
    friend bool operator>=(const FallbackVersion &fv, const VersionV1 &v1);
    friend bool operator<=(const FallbackVersion &fv, const VersionV1 &v1);

    friend bool operator==(const FallbackVersion &fv, const VersionV2 &v2);
    friend bool operator!=(const FallbackVersion &fv, const VersionV2 &v2);
    friend bool operator>(const FallbackVersion &fv, const VersionV2 &v2);
    friend bool operator<(const FallbackVersion &fv, const VersionV2 &v2);
    friend bool operator>=(const FallbackVersion &fv, const VersionV2 &v2);
    friend bool operator<=(const FallbackVersion &fv, const VersionV2 &v2);

    [[nodiscard]] int compareWithOtherVersion(const std::string &raw) const noexcept;
    std::vector<std::string> list;

private:
    [[nodiscard]] int compare(const FallbackVersion &other) const noexcept;
};

} // namespace linglong::package
