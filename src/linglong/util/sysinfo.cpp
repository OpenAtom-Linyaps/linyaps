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

/*!
 * query username with pid
 * @param uid
 * @return
 */
QString getUserName(uid_t uid)
{
    struct passwd *user = getpwuid(uid);
    if (user) {
        return QString::fromUtf8(user->pw_name);
    }
    qCritical() << "getUserName err";
    return "";
}

/*!
 * query current username
 * @return
 */
QString getUserName()
{
    return getUserName(geteuid());
}

} // namespace util
} // namespace linglong
