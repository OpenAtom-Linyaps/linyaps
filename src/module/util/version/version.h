/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_VERSION_VERSION_H_
#define LINGLONG_SRC_MODULE_UTIL_VERSION_VERSION_H_

#include <QString>
#include <QStringList>

namespace linglong {
namespace util {
// 默认最小版本号
const QString APP_MIN_VERSION = "0.0.0.0";

class AppVersion
{
public:
    explicit AppVersion(const QString &version)
    {
        QStringList verList = version.split(".");
        switch (verList.size()) {
        default:
        case 4:
            Build = verList.at(3).toInt();
        case 3:
            Revision = verList.at(2).toInt();
        case 2:
            Minor = verList.at(1).toInt();
        case 1:
            Major = verList.at(0).toInt();
        case 0:
            break;
        }
    }

    /*
     * 版本号是否有效
     *
     * @return bool: true:有效 false:无效
     */
    bool isValid() { return Major >= 0; }

    /*
     * 将当前版本号与给定的版本号比较
     *
     * @return bool: true:当前版本号比给定的版本号大 false:当前版本号比给定的版本号小
     */
    bool isBigThan(const AppVersion &ver)
    {
        if (Major > ver.Major) {
            return true;
        }
        if (Major == ver.Major && Minor > ver.Minor) {
            return true;
        }
        if (Major == ver.Major && Minor == ver.Minor && Revision > ver.Revision) {
            return true;
        }
        if (Major == ver.Major && Minor == ver.Minor && Revision == ver.Revision
            && Build > ver.Build) {
            return true;
        }
        return false;
    }

    /*
     * 版本号转QString
     */
    QString toString()
    {
        QString result = QString("%1.%2.%3.%4")
                                 .arg(QString::number(Major))
                                 .arg(QString::number(Minor))
                                 .arg(QString::number(Revision))
                                 .arg(QString::number(Build));
        return result;
    }

private:
    int Major = -1;
    int Minor = 0;
    int Revision = 0;
    int Build = 0;
};

} // namespace util
} // namespace linglong
#endif
