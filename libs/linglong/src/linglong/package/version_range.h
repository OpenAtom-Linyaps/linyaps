/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_VERSION_RANGE_H_
#define LINGLONG_PACKAGE_VERSION_RANGE_H_

#include "linglong/package/version.h"

#include <QString>

namespace linglong::package {

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
    explicit VersionRange(const std::tuple<QString, QString> &raw);
};
} // namespace linglong::package

#endif
