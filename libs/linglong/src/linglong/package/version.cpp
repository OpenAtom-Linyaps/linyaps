/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/package/version.h"
#include "linglong/package/fallback_version.h"
#include "linglong/package/versionv2.h"

#include <QRegularExpression>
#include <QString>
#include <QStringBuilder>
#include <variant>

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

utils::error::Result<Version> Version::parse(const QString &raw, const bool &fallback) noexcept
{
    LINGLONG_TRACE(QString("parse version %1").arg(raw));

    auto versionV2 = VersionV2::parse(raw);
    if (versionV2) {
        return Version(*versionV2);
    }

    if (!fallback) {
        return LINGLONG_ERR("parse version failed");
    }

    auto versionV1 = VersionV1::parse(raw);
    if (versionV1) {
        return Version(*versionV1);
    }

    auto fallbackVersion = FallbackVersion::parse(raw);
    if (fallbackVersion) {
        return Version(*fallbackVersion);
    }
    return LINGLONG_ERR("parse version failed");
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

    if (std::holds_alternative<VersionV2>(version)) {
        return std::get<VersionV2>(version).toString();
    }

    if (std::holds_alternative<FallbackVersion>(version)) {
        return std::get<FallbackVersion>(version).toString();
    }

    return QString("unknown version type");
}

utils::error::Result<VersionV1> VersionV1::parse(const QString &raw) noexcept
try {
    return VersionV1(raw);
} catch (const std::exception &e) {
    LINGLONG_TRACE("parse version " + raw);
    return LINGLONG_ERR(e);
}

VersionV1::VersionV1(const QString &raw)
{
    // modified from https://regex101.com/r/vkijKf/1/
    static auto regexExp = []() noexcept {
        QRegularExpression regexExp(
          R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$)");
        regexExp.optimize();
        return regexExp;
    }();

    QRegularExpressionMatch matched = regexExp.match(raw);

    if (!matched.hasMatch()) {
        throw std::runtime_error(
          "version regex mismatched, please use four digits version like 1.0.0.0");
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

bool VersionV1::operator==(const VersionV1 &that) const noexcept
{
    if (this->tweak.has_value() != that.tweak.has_value()) {
        return false;
    }

    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    return std::tie(this->major, this->minor, this->patch, thistweak)
      == std::tie(that.major, that.minor, that.patch, thattweak);
}

bool VersionV1::operator!=(const VersionV1 &that) const noexcept
{
    return !(*this == that);
}

bool VersionV1::operator<(const VersionV1 &that) const noexcept
{
    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, thistweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, thattweak);

    return thisAsNumber < thatAsNumber;
}

bool VersionV1::operator>(const VersionV1 &that) const noexcept
{
    auto thistweak = this->tweak.value_or(0);
    auto thattweak = that.tweak.value_or(0);

    auto thisAsNumber = std::tie(this->major, this->minor, this->patch, thistweak);
    auto thatAsNumber = std::tie(that.major, that.minor, that.patch, thattweak);

    return thisAsNumber > thatAsNumber;
}

bool VersionV1::operator<=(const VersionV1 &that) const noexcept
{
    return (*this == that) || (*this < that);
}

bool VersionV1::operator>=(const VersionV1 &that) const noexcept
{
    return (*this == that) || (*this > that);
}

bool VersionV1::operator==(const VersionV2 &that) const noexcept
{
    if (std::tie(major, minor, patch) != std::tie(that.major, that.minor, that.patch)) {
        return false;
    }

    // 2. V1 不能有非零 tweak
    bool v1HasNoTweak = !tweak.has_value() || tweak.value() == 0;
    // 3. V2 不能有预发布标识
    bool v2HasNoPrerelease = that.prerelease.empty();
    // 4. V2 不能有安全更新
    bool v2HasNoSecurity = that.security == 0;

    return v1HasNoTweak && v2HasNoPrerelease && v2HasNoSecurity;
}

bool VersionV1::operator!=(const VersionV2 &that) const noexcept
{
    return !(*this == that);
}

bool VersionV1::operator<(const VersionV2 &that) const noexcept
{
    if (std::tie(major, minor, patch) < std::tie(that.major, that.minor, that.patch)) {
        return true;
    }

    if (std::tie(major, minor, patch) == std::tie(that.major, that.minor, that.patch)) {
        // V1 不能有非零 tweak
        bool v1HasNoTweak = !tweak.has_value() || tweak.value() == 0;
        // V2 不能有预发布标识
        bool v2HasNoPrerelease = that.prerelease.empty();
        // V2 必须有安全更新
        bool v2HasSecurity = that.security != 0;

        return v1HasNoTweak && v2HasNoPrerelease && v2HasSecurity;
    }

    return false;
}

bool VersionV1::operator>(const VersionV2 &that) const noexcept
{
    return !(*this == that) && !(*this < that);
}

bool VersionV1::operator<=(const VersionV2 &that) const noexcept
{
    return (*this == that) || (*this < that);
}

bool VersionV1::operator>=(const VersionV2 &that) const noexcept
{
    return !(*this < that);
}

bool VersionV1::operator==(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) == 0;
}

bool VersionV1::operator!=(const FallbackVersion &that) const noexcept
{
    return !(*this == that);
}

bool VersionV1::operator<(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) > 0;
}

bool VersionV1::operator>(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) < 0;
}

bool VersionV1::operator<=(const FallbackVersion &that) const noexcept
{
    return *this == that || *this < that;
}

bool VersionV1::operator>=(const FallbackVersion &that) const noexcept
{
    return *this == that || *this > that;
}

QString VersionV1::toString() const noexcept
{
    return QString("%1.%2.%3%4")
      .arg(this->major)
      .arg(this->minor)
      .arg(this->patch)
      .arg(this->tweak ? "." + QString::number(*this->tweak) : "");
}
} // namespace linglong::package
