/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/versionv2.h"

#include "linglong/package/fallback_version.h"
#include "linglong/package/semver.hpp"
#include "linglong/package/version.h"

#include <QRegularExpression>

namespace linglong::package {
utils::error::Result<VersionV2> VersionV2::parse(const QString &raw) noexcept
{
    LINGLONG_TRACE("parse version v2 " + raw);
    try {
        auto semverVersion =
          semver::version::parse(raw.toStdString(), !(raw.startsWith('v') || raw.startsWith('V')));

        return VersionV2(semverVersion.major(),
                         semverVersion.minor(),
                         semverVersion.patch(),
                         semverVersion.prerelease(),
                         semverVersion.build_meta(),
                         semverVersion.security());
    } catch (const semver::semver_exception &e) {
        return LINGLONG_ERR(e.what());
    }
}

VersionV2::VersionV2(uint64_t major,
                     uint64_t minor,
                     uint64_t patch,
                     const std::string &prerelease,
                     std::string build_meta,
                     uint64_t security)
    : major(major)
    , minor(minor)
    , patch(patch)
    , prerelease(prerelease)
    , buildMeta(build_meta)
    , security(security)
{
}

bool VersionV2::operator==(const VersionV2 &that) const noexcept
{
    return semver::version(major, minor, patch, prerelease, buildMeta, security)
      == semver::version(that.major,
                         that.minor,
                         that.patch,
                         that.prerelease,
                         that.buildMeta,
                         that.security);
}

bool VersionV2::operator!=(const VersionV2 &that) const noexcept
{
    return !(*this == that);
}

bool VersionV2::operator<(const VersionV2 &that) const noexcept
{
    return semver::version(major, minor, patch, prerelease, buildMeta, security)
      < semver::version(that.major,
                        that.minor,
                        that.patch,
                        that.prerelease,
                        that.buildMeta,
                        that.security);
}

bool VersionV2::operator>(const VersionV2 &that) const noexcept
{
    return !(*this == that) && !(*this < that);
}

bool VersionV2::operator<=(const VersionV2 &that) const noexcept
{
    return (*this == that) || (*this < that);
}

bool VersionV2::operator>=(const VersionV2 &that) const noexcept
{
    return !(*this < that);
}

bool VersionV2::operator==(const VersionV1 &that) const noexcept
{
    if (std::tie(major, minor, patch) != std::tie(that.major, that.minor, that.patch)) {
        return false;
    }

    if (that.tweak.has_value() && that.tweak.value() != 0) {
        return false;
    }

    if (!prerelease.empty()) {
        return false;
    }

    if (security != 0) {
        return false;
    }
    return true;
}

bool VersionV2::operator!=(const VersionV1 &that) const noexcept
{
    return !(*this == that);
}

bool VersionV2::operator<(const VersionV1 &that) const noexcept
{
    return !(*this == that) && !(*this > that);
}

bool VersionV2::operator>(const VersionV1 &that) const noexcept
{
    if (std::tie(major, minor, patch) > std::tie(that.major, that.minor, that.patch)) {
        return true;
    }

    if (std::tie(major, minor, patch) < std::tie(that.major, that.minor, that.patch)) {
        return false;
    }

    if (that.tweak.has_value() && that.tweak.value() != 0) {
        return false;
    }

    if (!prerelease.empty()) {
        return false;
    }

    if (security != 0) {
        return true;
    }
}

bool VersionV2::operator<=(const VersionV1 &that) const noexcept
{
    return (*this == that) || !(*this > that);
}

bool VersionV2::operator>=(const VersionV1 &that) const noexcept
{
    return !(*this < that);
}

bool VersionV2::operator==(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) == 0;
}

bool VersionV2::operator!=(const FallbackVersion &that) const noexcept
{
    return !(*this == that);
}

bool VersionV2::operator<(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) > 0;
}

bool VersionV2::operator>(const FallbackVersion &that) const noexcept
{
    return that.compareWithOtherVersion(toString()) < 0;
}

bool VersionV2::operator<=(const FallbackVersion &that) const noexcept
{
    return *this == that || *this < that;
}

bool VersionV2::operator>=(const FallbackVersion &that) const noexcept
{
    return *this == that || *this > that;
}

QString VersionV2::toString() const noexcept
{
    return QString::fromStdString(
      semver::version(major, minor, patch, prerelease, buildMeta, security).str());
}

} // namespace linglong::package
