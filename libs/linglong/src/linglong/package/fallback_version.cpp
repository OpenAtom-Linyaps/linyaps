/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fallback_version.h"

#include "linglong/common/strings.h"
#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"

#include <fmt/format.h>

#include <charconv>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto SkipEmptyParts = QString::SkipEmptyParts;
} // namespace Qt
#endif

namespace {
bool parse_int_strict(const std::string &s, int &out)
{
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && ptr == s.data() + s.size();
}

} // namespace

namespace linglong::package {

utils::error::Result<FallbackVersion> FallbackVersion::parse(const std::string &raw) noexcept
{
    LINGLONG_TRACE(fmt::format("parse fallback version {}", raw));
    auto list = split(raw, '.', common::strings::splitOption::SkipEmpty);
    if (list.empty()) {
        return LINGLONG_ERR("parse fallback version failed");
    }
    return FallbackVersion(list);
}

bool FallbackVersion::semanticMatch(const std::string &versionStr) const noexcept
{
    auto versionParts =
      common::strings::split(versionStr, '.', common::strings::splitOption::SkipEmpty);
    if (versionParts.empty() || versionParts.size() > list.size()) {
        return false;
    }
    for (std::size_t i = 0; i < versionParts.size(); ++i) {
        if (list[i] != versionParts[i]) {
            return false;
        }
    }
    return true;
}

bool FallbackVersion::operator==(const FallbackVersion &that) const
{
    return this->list == that.list;
}

bool FallbackVersion::operator!=(const FallbackVersion &that) const
{
    return !(*this == that);
}

bool FallbackVersion::operator>(const FallbackVersion &that) const
{
    return compare(that) > 0;
}

bool FallbackVersion::operator<(const FallbackVersion &that) const
{
    return compare(that) < 0;
}

bool FallbackVersion::operator>=(const FallbackVersion &that) const
{
    return *this == that || *this > that;
}

bool FallbackVersion::operator<=(const FallbackVersion &that) const
{
    return *this == that || *this < that;
}

int FallbackVersion::compare(const FallbackVersion &other) const noexcept
{
    auto thisPart = this->list.cbegin();
    auto otherPart = other.list.cbegin();

    while (thisPart != this->list.cend() && otherPart != other.list.cend()) {
        int thisNumber{ -1 };
        int otherNumber{ -1 };
        const bool thisIsNum = parse_int_strict(*thisPart, thisNumber);
        const bool otherIsNum = parse_int_strict(*otherPart, otherNumber);

        // all numbers
        if (thisIsNum && otherIsNum) {
            if (thisNumber != otherNumber) {
                return (thisNumber < otherNumber) ? -1 : 1;
            }
        } else {
            // if the one of them is not a number
            // compare as strings
            if (*thisPart != *otherPart) {
                return thisPart->compare(*otherPart);
            }
        }

        ++thisPart;
        ++otherPart;
    }

    if (thisPart == this->list.cend() && otherPart == other.list.cend()) {
        return 0;
    }
    if (thisPart == this->list.cend()) {
        return -1;
    }
    if (otherPart == other.list.cend()) {
        return 1;
    }

    // unreachable
    assert(false);
    return 0;
}

std::string FallbackVersion::toString() const noexcept
{
    return common::strings::join(list, '.');
}

bool operator==(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 == fv;
}

bool operator!=(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 != fv;
}

bool operator>(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 < fv;
}

bool operator<(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 > fv;
}

bool operator>=(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 <= fv;
}

bool operator<=(const FallbackVersion &fv, const VersionV1 &v1)
{
    return v1 >= fv;
}

bool operator==(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 == fv;
}

bool operator!=(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 != fv;
}

bool operator>(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 < fv;
}

bool operator<(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 > fv;
}

bool operator>=(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 <= fv;
}

bool operator<=(const FallbackVersion &fv, const VersionV2 &v2)
{
    return v2 >= fv;
}

int FallbackVersion::compareWithOtherVersion(const std::string &raw) const noexcept
{
    auto result = FallbackVersion::parse(raw);
    if (!result) {
        return 0;
    }
    const FallbackVersion &other = *result;

    return this->compare(other);
}

} // namespace linglong::package
