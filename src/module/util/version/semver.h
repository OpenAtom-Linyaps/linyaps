/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_VERSION_SEMVER_H_
#define LINGLONG_SRC_MODULE_UTIL_VERSION_SEMVER_H_

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "module/package/ref.h"

namespace linglong {
namespace util {

const QString regex =
    "^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)(?:\\.(0|[1-9]\\d*))?(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?$";

// semver2.0 regex matches major.minor.patch-prerelease+buildinfo. https://semver.org/spec/v2.0.0.html
// linglong regex matches major.minor.patch.unknow-prerelease+buildinfo

/*!
 * 判断版本号是否满足规范
 *
 * @param version: 软件包版本号
 *
 * @return bool：true:合法 false: 不合法
 */
bool isRegular(const QString &version)
{
    QRegularExpression regexExp(regex);
    QRegularExpressionMatch matched = regexExp.match(version);

    return matched.hasMatch();
}

/*!
 * 按照semver2.0正则表达式拆解版本号，返回按组拆解的版本号
 *
 * @param version: 软件包版本号: 1.2.3.4
 *
 * @return QStringList：{1,2,3,4, "", ""}
 */
QStringList matchGroup(const QString &version)
{
    QStringList matchVersion = {"-1", "-1", "-1", "-1", "", ""};
    QRegularExpression regexExp(regex);
    QRegularExpressionMatch matched = regexExp.match(version);

    for (int index = 1; index <= matched.lastCapturedIndex(); ++index) {
        matchVersion.replace(index - 1, matched.captured(index));
    }

    return matchVersion;
}

/*!
 * 比较版本号大小
 *
 * @param first: 第一个版本号
 * @param second: 第二个版本号
 *
 * @return int：大于返回1；相等返回0;小于返回-1;
 */
int compareVersion(const QString &first, const QString &second)
{
    auto firstGroup = matchGroup(first);
    auto secondGroup = matchGroup(second);

    // 版本号最多有六位，前四位为主版本号，第五位为预发布版本号，最后一位为构建信息，构建信息位不参与版本比较
    for (int index = 0; index < 4; ++index) {
        if (firstGroup.at(index).toInt() == secondGroup.at(index).toInt()) {
            continue;
        }

        if (firstGroup.at(index).toInt() > secondGroup.at(index).toInt()) {
            return 1;
        } else {
            return -1;
        }
    }
    // 当前四位主版本号均相同时，比较预发布版本的ascii码
    if (QString::compare(firstGroup.at(4), secondGroup.at(4)) > 0) {
        return 1;
    } else if (QString::compare(firstGroup.at(4), secondGroup.at(4)) < 0) {
        return -1;
    }

    return 0;
}

/*!
 * 比较版本号大小
 *
 * @param refStrList: ref列表, 如：linglong/demo/1.1/x86_64/runtime
 *
 * @return QString：最大版本号
 */
QString latestVersion(const QStringList &refStrList)
{
    QString latestVer = "latest";
    for (auto refStr : refStrList) {
        auto tmpRef = package::Ref(refStr);
        latestVer = compareVersion(latestVer, tmpRef.version) > 0 ? latestVer : tmpRef.version;
    }

    return latestVer;
}

} // namespace util
} // namespace linglong
#endif
