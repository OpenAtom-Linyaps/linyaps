/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/package/version.h"

#include <QString>

namespace linglong::package {
// TODO: This class is currently not used. Consider using it in the future or removing it if not
// needed.
class VersionRange final
{
public:
    explicit VersionRange(const QString &raw);
    static utils::error::Result<VersionRange> parse(const QString &raw) noexcept;
    Version begin;
    Version end;

    QString toString() const noexcept;

    bool contains(const Version &ver) const noexcept;

private:
    explicit VersionRange(const std::tuple<Version, Version> &raw);
};
} // namespace linglong::package
