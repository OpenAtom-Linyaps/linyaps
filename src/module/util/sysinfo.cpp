/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "sysinfo.h"

#include <QDebug>
#include <QSysInfo>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace linglong {
namespace util {

QString hostArch()
{
    // https://doc.qt.io/qt-5/qsysinfo.html#currentCpuArchitecture
    // "arm64"
    // "mips64"
    // "x86_64"
    QString arch = QSysInfo::currentCpuArchitecture();
    // FIXME: the support arch should be list. and how to extern an new arch?
    return arch;
}

/*
 * 查询当前登陆用户名
 *
 * @return QString: 当前登陆用户名
 */
QString getUserName()
{
    uid_t uid = geteuid();
    struct passwd *user = getpwuid(uid);
    QString userName = "";
    if (user && user->pw_name) {
        userName = QString(QLatin1String(user->pw_name));
    } else {
        qCritical() << "getUserName err";
    }
    return userName;
}
} // namespace util
} // namespace linglong
