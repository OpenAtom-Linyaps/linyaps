/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/versionv2.h"

#include "linglong/package/semver.hpp"
#include "linglong/package/version.h"

#include <QRegularExpression>

namespace linglong::package {
utils::error::Result<VersionV2> VersionV2::parse(const QString &raw) noexcept
{
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
        LINGLONG_TRACE("parse versionv2 " + raw);
        return LINGLONG_ERR(e);
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

bool VersionV2::operator==(const Version &that) const noexcept
{
    return std ::tie(major, minor, patch) == std::tie(that.major, that.minor, that.patch)
      && prerelease.empty() && security == 0;
}

bool VersionV2::operator!=(const Version &that) const noexcept
{
    return !(*this == that);
}

bool VersionV2::operator<(const Version &that) const noexcept
{
    if (std::tie(major, minor, patch) < std::tie(that.major, that.minor, that.patch)) {
        return true;
    }
    if (std::tie(major, minor, patch) == std::tie(that.major, that.minor, that.patch)) {
        if (!prerelease.empty()) {
            return true;
        }
    }
    return false;
}

bool VersionV2::operator>(const Version &that) const noexcept
{
    if (std::tie(major, minor, patch) > std::tie(that.major, that.minor, that.patch)) {
        return true;
    }
    if (std::tie(major, minor, patch) == std::tie(that.major, that.minor, that.patch)) {
        if (prerelease.empty() && security) {
            return true;
        }
    }
    return false;
}

bool VersionV2::operator<=(const Version &that) const noexcept
{
    return (*this == that) || !(*this > that);
}

bool VersionV2::operator>=(const Version &that) const noexcept
{
    return !(*this < that);
}

QString VersionV2::toString() const noexcept
{
    return QString::fromStdString(
      semver::version(major, minor, patch, prerelease, buildMeta, security).str());
}

} // namespace linglong::package
