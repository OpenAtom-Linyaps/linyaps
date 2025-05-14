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

#include <variant>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto SkipEmptyParts = QString::SkipEmptyParts;
} // namespace Qt
#endif

namespace linglong::package {

utils::error::Result<Version> Version::parse(const QString &raw,
                                             const ParseOptions parseOpt) noexcept
{
    LINGLONG_TRACE(QString("parse version %1").arg(raw));

    auto versionV2 = VersionV2::parse(raw, parseOpt.strict);
    if (versionV2) {
        return Version(*versionV2);
    }

    if (!parseOpt.fallback) {
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

utils::error::Result<void> Version::validateDependVersion(const QString &raw) noexcept
{
    LINGLONG_TRACE(QString{ "validate depend version %1" }.arg(raw));
    static auto regexExp = []() noexcept {
        QRegularExpression regexExp(R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$)");
        regexExp.optimize();
        return regexExp;
    }();

    QRegularExpressionMatch matched = regexExp.match(raw);
    if (!matched.hasMatch()) {
        return LINGLONG_ERR(
          "version regex mismatched, please use three digits version like MAJOR.MINOR[.PATCH]");
    }
    return LINGLONG_OK;
}

std::vector<linglong::api::types::v1::PackageInfoV2> Version::filterByFuzzyVersion(
  std::vector<linglong::api::types::v1::PackageInfoV2> list, const QString &fuzzyVersion)
{
    for (auto it = list.begin(); it != list.end();) {
        auto packageVerRet = package::Version::parse(it->version.c_str());
        if (!packageVerRet) {
            qWarning() << "Ignore invalid package record " << packageVerRet.error();
            it = list.erase(it);
            continue;
        }

        if (!packageVerRet->semanticMatch(fuzzyVersion)) {
            it = list.erase(it);
            continue;
        }
        ++it;
    }
    return list;
}

bool Version::semanticMatch(const QString &versionStr)
{
    if (std::holds_alternative<VersionV1>(version)) {
        return std::get<VersionV1>(version).semanticMatch(versionStr);
    }

    if (std::holds_alternative<VersionV2>(version)) {
        return std::get<VersionV2>(version).semanticMatch(versionStr);
    }

    if (std::holds_alternative<FallbackVersion>(version)) {
        return std::get<FallbackVersion>(version).semanticMatch(versionStr);
    }

    return false;
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
} // namespace linglong::package
