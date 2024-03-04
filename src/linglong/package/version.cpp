/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/version.h"

#include <QStringBuilder>

namespace linglong::package {
namespace {

struct PreRelease
{
    explicit PreRelease(const QString &raw)
        : list(raw.split('.', Qt::SkipEmptyParts))
    {
    }

    bool operator==(const PreRelease &that) const { return this->list == that.list; };

    bool operator<(const PreRelease &that) const
    {
        auto limit = std::min(this->list.size(), that.list.size());
        for (decltype(limit) i = 0; i < limit; i++) {
            bool ok = false;
            auto thisAsNumber = this->list[i].toInt(&ok);
            if (!ok) {
                thisAsNumber = std::numeric_limits<decltype(thisAsNumber)>::max();
            }
            auto thatAsNumber = that.list[i].toInt(&ok);
            if (!ok) {
                thatAsNumber = std::numeric_limits<decltype(thatAsNumber)>::max();
            }

            if (thisAsNumber != thatAsNumber) {
                return thisAsNumber < thatAsNumber;
            }

            if (this->list[i] != that.list[i]) {
                return this->list[i] < that.list[i];
            }
        }

        if (this->list.isEmpty()) {
            return false;
        }

        if (that.list.isEmpty()) {
            return !this->list.isEmpty();
        }

        return this->list.size() < that.list.size();
    }

    bool operator>(const PreRelease &that) const
    {
        auto limit = std::min(this->list.size(), that.list.size());
        for (decltype(limit) i = 0; i < limit; i++) {
            bool ok = false;
            auto thisAsNumber = this->list[i].toInt(&ok);
            if (!ok) {
                thisAsNumber = std::numeric_limits<decltype(thisAsNumber)>::max();
            }
            auto thatAsNumber = that.list[i].toInt(&ok);
            if (!ok) {
                thatAsNumber = std::numeric_limits<decltype(thatAsNumber)>::max();
            }

            if (thisAsNumber != thatAsNumber) {
                return thisAsNumber > thatAsNumber;
            }

            if (this->list[i] != that.list[i]) {
                return this->list[i] > that.list[i];
            }
        }

        if (this->list.isEmpty()) {
            return !that.list.isEmpty();
        }

        if (that.list.isEmpty()) {
            return false;
        }

        return this->list.size() > that.list.size();
    }

    QStringList list;
};

} // namespace

utils::error::Result<Version> Version::parse(const QString &raw) noexcept
try {
    return Version(raw);
} catch (const std::exception &e) {
    LINGLONG_TRACE("parse version");
    return LINGLONG_ERR(e);
}

Version::Version(const QString &raw)
{
    // modified from https://regex101.com/r/vkijKf/1/
    static QRegularExpression regexExp(
      R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");

    QRegularExpressionMatch matched = regexExp.match(raw);

    if (!matched.hasMatch()) {
        throw std::runtime_error("version regex mismatched");
    }

    bool ok = false;
    this->major = matched.captured(1).toLongLong(&ok);
    if (!ok) {
        throw std::runtime_error("major too large");
    }
    this->minor = matched.captured(2).toLongLong(&ok);
    if (!ok) {
        throw std::runtime_error("minor too large");
    }
    this->patch = matched.captured(3).toLongLong(&ok);
    if (!ok) {
        throw std::runtime_error("patch too large");
    }
    this->tweak = matched.captured(4).toLongLong(&ok);
    if (!ok) {
        throw std::runtime_error("tweak too large");
    }

    this->preRelease = matched.captured(5);
    this->build = matched.captured(6);
}

bool Version::operator==(const Version &that) const noexcept
{
    return std::tie(this->major, this->minor, this->patch, this->tweak, this->preRelease)
      == std::tie(that.major, that.minor, that.patch, that.tweak, that.preRelease);
}

bool Version::operator!=(const Version &that) const noexcept
{
    return !(*this == that);
}

bool Version::operator<(const Version &that) const noexcept
{
    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, this->tweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, that.tweak);

    if (thisAsNumber == thatAsNumber) {
        return PreRelease(this->preRelease) < PreRelease(that.preRelease);
    }

    return thisAsNumber < thatAsNumber;
}

bool Version::operator>(const Version &that) const noexcept
{
    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, this->tweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, that.tweak);

    if (thisAsNumber == thatAsNumber) {
        auto thisPreRelease = PreRelease(this->preRelease);
        auto thatPreRelease = PreRelease(that.preRelease);

        return thisPreRelease > thatPreRelease;
    }

    return thisAsNumber > thatAsNumber;
}

bool Version::operator<=(const Version &that) const noexcept
{
    return (*this == that) || (*this < that);
}

bool Version::operator>=(const Version &that) const noexcept
{
    return (*this == that) || (*this > that);
}

QString Version::toString() const noexcept
{
    return QString("%1.%2.%3.%4")
             .arg(this->major)
             .arg(this->minor)
             .arg(this->patch)
             .arg(this->tweak)
      % (this->preRelease.isEmpty() ? QString("") : "-" % this->preRelease)
      % (this->build.isEmpty() ? QString("") : "+" % this->build);
}
} // namespace linglong::package
