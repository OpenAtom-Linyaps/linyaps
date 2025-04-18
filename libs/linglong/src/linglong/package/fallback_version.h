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
    static utils::error::Result<FallbackVersion> parse(const QString &raw) noexcept;

    explicit FallbackVersion(const QStringList &list)
        : list(list) { };

    QString toString() const noexcept;

    bool operator==(const FallbackVersion &that) const;
    bool operator!=(const FallbackVersion &that) const;
    bool operator>(const FallbackVersion &that) const;
    bool operator<(const FallbackVersion &that) const;
    bool operator>=(const FallbackVersion &that) const;
    bool operator<=(const FallbackVersion &that) const;

    bool operator==(const VersionV1 &that) const;
    bool operator!=(const VersionV1 &that) const;
    bool operator>(const VersionV1 &that) const;
    bool operator<(const VersionV1 &that) const;
    bool operator>=(const VersionV1 &that) const;
    bool operator<=(const VersionV1 &that) const;

    bool operator==(const VersionV2 &that) const;
    bool operator!=(const VersionV2 &that) const;
    bool operator>(const VersionV2 &that) const;
    bool operator<(const VersionV2 &that) const;
    bool operator>=(const VersionV2 &that) const;
    bool operator<=(const VersionV2 &that) const;

    int compareWithOtherVersion(const QString &raw) const;

    QStringList list;

private:
    int compare(const FallbackVersion &other) const noexcept;
};

} // namespace linglong::package
