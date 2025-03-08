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
    : VersionRange([&raw]() -> std::tuple<Version, Version> {
        static auto regExp = []() noexcept {
            QRegularExpression reg(R"(\[([^,]+),([^\)]+)\))");
            reg.optimize();
            return reg;
        }();
        auto matched = regExp.match(raw);

        if (!matched.hasMatch()) {
            throw std::invalid_argument("Invalid version range format: " + raw.toStdString());
        }

        auto begin = Version::parse(matched.captured(1));
        if (!begin) {
            throw std::invalid_argument("Failed to parse begin version: "
                                        + matched.captured(1).toStdString());
        }

        auto end = Version::parse(matched.captured(2));
        if (!end) {
            throw std::invalid_argument("Failed to parse end version: "
                                        + matched.captured(2).toStdString());
        }

        return std::make_tuple(*begin, *end);
    }())
{
}

VersionRange::VersionRange(const std::tuple<Version, Version> &raw)
    : begin(std::get<0>(raw))
    , end(std::get<1>(raw))
{
}

QString VersionRange::toString() const noexcept
{
    return QString("[%1,%2)").arg(this->begin.toString(), this->end.toString());
}

bool VersionRange::contains(const Version &ver) const noexcept
{
    return this->begin <= ver && ver < this->end;
}

} // namespace linglong::package
