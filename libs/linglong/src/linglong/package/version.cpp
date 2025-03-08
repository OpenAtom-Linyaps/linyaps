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

// TODO: This class has no practical use and can be removed
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

utils::error::Result<Version> Version::parse(const QString &raw,
                                             const ParseOptions parseOpt) noexcept
{
    LINGLONG_TRACE("parse version " + raw);

    auto versionV2 = VersionV2::parse(raw, parseOpt.strict);
    if (versionV2) {
        return Version(*versionV2);
    }

    if (!parseOpt.fallback) {
        return LINGLONG_ERR("parse version failed");
    }

    auto versionV1 = VersionV1::parse(raw);
    if (!versionV1) {
        return LINGLONG_ERR("parse version failed");
    }

    return Version(*versionV1);
}

utils::error::Result<void> Version::validateDependVersion(const QString &raw) noexcept
{
    LINGLONG_TRACE(QString{"validate depend version %1"}.arg(raw));
    static auto regexExp = []() noexcept {
        QRegularExpression regexExp(
          R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$)");
        regexExp.optimize();
        return regexExp;
    }();

    QRegularExpressionMatch matched = regexExp.match(raw);
    if (!matched.hasMatch()) {
        return LINGLONG_ERR("version regex mismatched, please use three digits version like MAJOR.MINOR[.PATCH]");
    }
    return LINGLONG_OK;
}

void Version::ignoreTweak() noexcept
{
    if (std::holds_alternative<VersionV1>(version)) {
        std::get<VersionV1>(version).tweak = std::nullopt;
    }
}

bool Version::isVersionV1() noexcept
{
    return std::holds_alternative<VersionV1>(version);
}

bool Version::hasTweak() noexcept
{
    if (std::holds_alternative<VersionV1>(version)) {
        return std::get<VersionV1>(version).tweak.has_value();
    }
    return false;
}

bool Version::operator==(const Version &that) const noexcept
{
    return std::visit(
      [&](const auto &thisVersion, const auto &thatVersion) {
          return thisVersion == thatVersion;
      },
      this->version,
      that.version);
}

bool Version::operator!=(const Version &that) const noexcept
{
    return !(*this == that);
}

bool Version::operator<(const Version &that) const noexcept
{
    return std::visit(
      [&](const auto &thisVersion, const auto &thatVersion) {
          return thisVersion < thatVersion;
      },
      this->version,
      that.version);
}

bool Version::operator>(const Version &that) const noexcept
{
    return !(*this == that) && !(*this < that);
}

bool Version::operator<=(const Version &that) const noexcept
{
    return (*this == that) || (*this < that);
}

bool Version::operator>=(const Version &that) const noexcept
{
    return !(*this < that);
}

QString Version::toString() const noexcept
{
    if (std::holds_alternative<VersionV1>(version)) {
        return std::get<VersionV1>(version).toString();
    }
    return std::get<VersionV2>(version).toString();
}

} // namespace linglong::package
