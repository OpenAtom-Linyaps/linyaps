/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <QString>

#include <optional>

namespace linglong::package {

// This is a 4 number semver
class Version final
{
public:
    static utils::error::Result<Version> parse(const QString &raw) noexcept;
    explicit Version(const QString &raw);

    qlonglong major = 0;
    qlonglong minor = 0;
    qlonglong patch = 0;
    std::optional<qlonglong> tweak = {};

    bool operator==(const Version &that) const noexcept;
    bool operator!=(const Version &that) const noexcept;
    bool operator<(const Version &that) const noexcept;
    bool operator>(const Version &that) const noexcept;
    bool operator<=(const Version &that) const noexcept;
    bool operator>=(const Version &that) const noexcept;

    QString toString() const noexcept;
};
} // namespace linglong::package
