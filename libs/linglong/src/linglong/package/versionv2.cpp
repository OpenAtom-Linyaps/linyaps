/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/versionv2.h"

#include "linglong/package/semver.hpp"
#include "linglong/package/versionv1.h"

#include <QRegularExpression>

namespace linglong::package {
utils::error::Result<VersionV2> VersionV2::parse(const QString &raw, bool strict) noexcept
{
    LINGLONG_TRACE("parse version v2 " + raw);
    try {
        auto semverVersion =
          semver::version::parse(raw.toStdString(), strict);

        return VersionV2(semverVersion.major(),
                         semverVersion.minor(),
                         semverVersion.patch(),
                         semverVersion.prerelease(),
                         semverVersion.build_meta(),
                         semverVersion.security(),
                         semverVersion.has_patch());
    } catch (const semver::semver_exception &e) {
        return LINGLONG_ERR(e.what());
    }
}

VersionV2::VersionV2(uint64_t major,
                     uint64_t minor,
                     uint64_t patch,
                     const std::string &prerelease,
                     std::string buildMeta,
                     uint64_t security,
                     bool hasPatch) noexcept
    : major(major)
    , minor(minor)
    , patch(patch)
    , prerelease(prerelease)
    , buildMeta(buildMeta)
    , security(security)
    , hasPatch(hasPatch)
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

bool operator==(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return v1 == v2;
}

bool operator!=(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return !(v1 == v2);
}

bool operator<(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return v1 > v2;
}

bool operator>(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return v1 < v2;
}

bool operator<=(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return v1 >= v2;
}

bool operator>=(const VersionV2 &v2, const VersionV1 &v1) noexcept
{
    return v1 <= v2;
}

QString VersionV2::toString() const noexcept
{
    return QString::fromStdString(
      semver::version(major, minor, patch, prerelease, buildMeta, security).str());
}

} // namespace linglong::package
