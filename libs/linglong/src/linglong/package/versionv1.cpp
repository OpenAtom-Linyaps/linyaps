/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

namespace linglong::package {
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

bool operator==(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    if (std::tie(v1.major, v1.minor, v1.patch) != std::tie(v2.major, v2.minor, v2.patch)) {
        return false;
    }

    // 2. V1 不能有非零 tweak
    bool v1HasNoTweak = !v1.tweak.has_value();
    // 3. V2 不能有预发布标识
    bool v2HasNoPrerelease = v2.prerelease.empty();
    // 4. V2 不能有安全更新
    bool v2HasNoSecurity = v2.security == 0;

    return v1HasNoTweak && v2HasNoPrerelease && v2HasNoSecurity;
}

bool operator!=(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    return !(v1 == v2);
}

bool operator<(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    if (std::tie(v1.major, v1.minor, v1.patch) < std::tie(v2.major, v2.minor, v2.patch)) {
        return true;
    }

    if (std::tie(v1.major, v1.minor, v1.patch) == std::tie(v2.major, v2.minor, v2.patch)) {
        // V1 不能有非零 tweak
        bool v1HasNoTweak = !v1.tweak.has_value();
        // V2 不能有预发布标识
        bool v2HasNoPrerelease = v2.prerelease.empty();
        // V2 必须有安全更新
        bool v2HasSecurity = v2.security != 0;

        return v1HasNoTweak && v2HasNoPrerelease && v2HasSecurity;
    }

    return false;
}

bool operator>(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    return !(v1 == v2) && !(v1 < v2);
}

bool operator<=(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    return (v1 == v2) || (v1 < v2);
}

bool operator>=(const VersionV1 &v1, const VersionV2 &v2) noexcept
{
    return !(v1 < v2);
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
