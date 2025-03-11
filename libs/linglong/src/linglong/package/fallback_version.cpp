/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fallback_version.h"

#include "linglong/package/version.h"
#include "linglong/package/versionv2.h"

namespace linglong::package {

utils::error::Result<FallbackVersion> FallbackVersion::parse(const QString &raw) noexcept
{
    LINGLONG_TRACE(QString("parse fallback version %1").arg(raw));
    QStringList list = raw.split('.', Qt::SkipEmptyParts);
    if (list.isEmpty()) {
        return LINGLONG_ERR("parse fallback version failed");
    }
    return FallbackVersion(list);
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
    for (int i = 0; i < std::max(list.size(), other.list.size()); ++i) {
        QString thisPart = list.value(i, "0");
        QString otherPart = other.list.value(i, "0");

        // 检查是否为数字
        bool thisIsNumber = false;
        bool otherIsNumber = false;
        int thisNumber = thisPart.toInt(&thisIsNumber);
        int otherNumber = otherPart.toInt(&otherIsNumber);

        if (thisIsNumber && otherIsNumber) {
            // 如果都是数字，按数值比较
            if (thisNumber != otherNumber) {
                return thisNumber - otherNumber;
            }
        } else {
            // 如果至少有一个不是数字，按字符串比较
            if (thisPart != otherPart) {
                return thisPart.compare(otherPart);
            }
        }
    }
    return 0;
}

QString FallbackVersion::toString() const noexcept
{
    return this->list.join(".");
}

bool FallbackVersion::operator==(const VersionV1 &that) const
{
    return compareWithOtherVersion(that.toString()) == 0;
}

bool FallbackVersion::operator!=(const VersionV1 &that) const
{
    return !(*this == that);
}

bool FallbackVersion::operator>(const VersionV1 &that) const
{
    return compareWithOtherVersion(that.toString()) > 0;
}

bool FallbackVersion::operator<(const VersionV1 &that) const
{
    return compareWithOtherVersion(that.toString()) < 0;
}

bool FallbackVersion::operator>=(const VersionV1 &that) const
{
    return *this == that || *this > that;
}

bool FallbackVersion::operator<=(const VersionV1 &that) const
{
    return *this == that || *this < that;
}

bool FallbackVersion::operator==(const VersionV2 &that) const
{
    return compareWithOtherVersion(that.toString()) == 0;
}

bool FallbackVersion::operator!=(const VersionV2 &that) const
{
    return !(*this == that);
}

bool FallbackVersion::operator>(const VersionV2 &that) const
{
    return compareWithOtherVersion(that.toString()) > 0;
}

bool FallbackVersion::operator<(const VersionV2 &that) const
{
    return compareWithOtherVersion(that.toString()) < 0;
}

bool FallbackVersion::operator>=(const VersionV2 &that) const
{
    return *this == that || *this > that;
}

bool FallbackVersion::operator<=(const VersionV2 &that) const
{
    return *this == that || *this < that;
}

bool FallbackVersion::compareWithOtherVersion(const QString &raw) const
{
    auto result = FallbackVersion::parse(raw);
    if (!result) {
        return false;
    }
    const FallbackVersion &other = *result;

    for (int i = 0; i < std::max(list.size(), other.list.size()); ++i) {
        QString thisPart = list.value(i, "0");
        QString otherPart = other.list.value(i, "0");

        bool thisIsNumber = false;
        bool otherIsNumber = false;
        int thisNumber = thisPart.toInt(&thisIsNumber);
        int otherNumber = otherPart.toInt(&otherIsNumber);

        if (thisIsNumber && otherIsNumber) {
            if (thisNumber != otherNumber) {
                return thisNumber - otherNumber;
            }
        } else {
            if (thisPart != otherPart) {
                return thisPart.compare(otherPart);
            }
        }
    }
    return true;
}

} // namespace linglong::package
