/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/version_range.h"

#include <QRegularExpression>

namespace linglong::package {

utils::error::Result<VersionRange> VersionRange::parse(const QString &raw) noexcept
try {
    return VersionRange(raw);
} catch (const std::exception &e) {
    LINGLONG_TRACE("parse version range");
    return LINGLONG_ERR(e);
}

VersionRange::VersionRange(const QString &raw)
    : VersionRange([&raw]() -> std::tuple<QString, QString> {
        static auto regExp = QRegularExpression(R"(\[([^,]+),([^\)]+)\))");
        auto matched = regExp.match(raw);
        return std::make_tuple(matched.captured(1), matched.captured(2));
    }())
{
}

VersionRange::VersionRange(const std::tuple<QString, QString> &raw)
    : begin(std::get<0>(raw))
    , end(std::get<1>(raw))
{
}

QString VersionRange::toString() const noexcept
{
    return QString("[%1,%2)").arg(this->begin.toString()).arg(this->end.toString());
}

bool VersionRange::contains(const Version &ver) const noexcept
{
    return this->begin <= ver && ver < this->end;
}

} // namespace linglong::package
