/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fallback_version.h"

#include "linglong/common/strings.h"
#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto SkipEmptyParts = QString::SkipEmptyParts;
} // namespace Qt
#endif

namespace {
std::optional<std::string_view> normalizeUnsignedInteger(std::string_view value) noexcept
{
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isdigit(ch);
        })) {
        return std::nullopt;
    }

    auto firstNonZero = value.find_first_not_of('0');
    if (firstNonZero == std::string_view::npos) {
        return value.substr(value.size() - 1);
    }

    return value.substr(firstNonZero);
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

    return FallbackVersion(std::vector<std::string>(list.cbegin(), list.cend()));
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
    return compare(that) == 0;
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
        auto thisNumber = normalizeUnsignedInteger(*thisPart);
        auto otherNumber = normalizeUnsignedInteger(*otherPart);

        // all numbers
        if (thisNumber && otherNumber) {
            if (thisNumber->size() != otherNumber->size()) {
                return thisNumber->size() < otherNumber->size() ? -1 : 1;
            }
            if (*thisNumber != *otherNumber) {
                return thisNumber->compare(*otherNumber);
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
