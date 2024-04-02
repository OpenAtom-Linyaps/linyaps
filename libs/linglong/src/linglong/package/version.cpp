/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/package/version.h"

#include <QRegularExpression>
#include <QString>
#include <QStringBuilder>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto SkipEmptyParts = QString::SkipEmptyParts;
} // namespace Qt
#endif

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
    LINGLONG_TRACE("parse version " + raw);
    return LINGLONG_ERR(e);
}

Version::Version(const QString &raw)
{
    // modified from https://regex101.com/r/vkijKf/1/
    static QRegularExpression regexExp(
      R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$)");

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
    if (!matched.captured(4).isNull()) {
        this->tweak = matched.captured(4).toLongLong(&ok);
        if (!ok) {
            throw std::runtime_error("tweak too large");
        }
    }
}

bool Version::operator==(const Version &that) const noexcept
{
    if (this->tweak.has_value() != that.tweak.has_value()) {
        return false;
    }

    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    return std::tie(this->major, this->minor, this->patch, thistweak)
      == std::tie(that.major, that.minor, that.patch, thattweak);
}

bool Version::operator!=(const Version &that) const noexcept
{
    return !(*this == that);
}

bool Version::operator<(const Version &that) const noexcept
{
    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, thistweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, thattweak);

    return thisAsNumber < thatAsNumber;
}

bool Version::operator>(const Version &that) const noexcept
{
    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, thistweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, thattweak);

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
    return QString("%1.%2.%3%4")
      .arg(this->major)
      .arg(this->minor)
      .arg(this->patch)
      .arg(this->tweak ? "." + QString::number(*this->tweak) : "");
}
} // namespace linglong::package
