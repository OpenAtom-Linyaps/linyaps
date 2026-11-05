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

#include <cctype>
#include <string_view>

namespace {
bool isDigit(char ch) noexcept
{
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

// "000" -> "0", "0021" -> "21"; value must be non-empty and contain only digits.
std::string_view stripLeadingZeros(std::string_view digits) noexcept
{
    const auto firstNonZero = digits.find_first_not_of('0');
    if (firstNonZero == std::string_view::npos) {
        return digits.empty() ? digits : digits.substr(digits.size() - 1);
    }

    return digits.substr(firstNonZero);
}

// Natural ordering: runs of digits are compared numerically (unbounded, leading
// zeros ignored), everything else is compared byte by byte. At a position where
// one side is a digit and the other is not, the digit sorts first. Two runs with
// the same numeric value are ordered by their raw spelling, so different textual
// forms (e.g. "02" and "2") stay distinct. This makes compare()==0 equivalent to
// plain string equality, which the version comparators and hash containers rely
// on.
int comparePart(std::string_view lhs, std::string_view rhs) noexcept
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < lhs.size() && j < rhs.size()) {
        const bool lhsDigit = isDigit(lhs[i]);
        const bool rhsDigit = isDigit(rhs[j]);

        if (lhsDigit && rhsDigit) {
            auto lhsEnd = i;
            while (lhsEnd < lhs.size() && isDigit(lhs[lhsEnd])) {
                ++lhsEnd;
            }
            auto rhsEnd = j;
            while (rhsEnd < rhs.size() && isDigit(rhs[rhsEnd])) {
                ++rhsEnd;
            }

            const auto lhsRaw = lhs.substr(i, lhsEnd - i);
            const auto rhsRaw = rhs.substr(j, rhsEnd - j);
            const auto lhsNumber = stripLeadingZeros(lhsRaw);
            const auto rhsNumber = stripLeadingZeros(rhsRaw);

            if (lhsNumber.size() != rhsNumber.size()) {
                return lhsNumber.size() < rhsNumber.size() ? -1 : 1;
            }
            if (const int cmp = lhsNumber.compare(rhsNumber); cmp != 0) {
                return cmp < 0 ? -1 : 1;
            }
            // Same numeric value: order by the raw spelling so that "02" and "2"
            // are not considered equal.
            if (lhsRaw != rhsRaw) {
                return lhsRaw.compare(rhsRaw) < 0 ? -1 : 1;
            }

            i = lhsEnd;
            j = rhsEnd;
            continue;
        }

        if (lhsDigit != rhsDigit) {
            return lhsDigit ? -1 : 1;
        }

        if (lhs[i] != rhs[j]) {
            const auto lhsChar = static_cast<unsigned char>(lhs[i]);
            const auto rhsChar = static_cast<unsigned char>(rhs[j]);
            return lhsChar < rhsChar ? -1 : 1;
        }

        ++i;
        ++j;
    }

    if (i < lhs.size()) {
        return 1;
    }
    if (j < rhs.size()) {
        return -1;
    }
    return 0;
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
        if (const int result = comparePart(*thisPart, *otherPart); result != 0) {
            return result;
        }

        ++thisPart;
        ++otherPart;
    }

    if (thisPart == this->list.cend() && otherPart == other.list.cend()) {
        return 0;
    }
    return thisPart == this->list.cend() ? -1 : 1;
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
