/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fallback_version.h"

#include "linglong/package/version.h"
#include "linglong/package/versionv2.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto SkipEmptyParts = QString::SkipEmptyParts;
} // namespace Qt
#endif

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

bool FallbackVersion::semanticMatch(const QString &versionStr) const noexcept
{
    QStringList versionParts = versionStr.split('.', Qt::SkipEmptyParts);
    if (versionParts.isEmpty() || versionParts.size() > list.size()) {
        return false;
    }
    for (int i = 0; i < versionParts.size(); ++i) {
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
    for (int i = 0; i < std::max(list.size(), other.list.size()); ++i) {
        QString thisPart = list.value(i, "");
        QString otherPart = other.list.value(i, "");

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

int FallbackVersion::compareWithOtherVersion(const QString &raw) const
{
    auto result = FallbackVersion::parse(raw);
    if (!result) {
        return 0;
    }
    const FallbackVersion &other = *result;

    return this->compare(other);
}

} // namespace linglong::package
