/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sysinfo.h"

#include <QSysInfo>

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

} // namespace util
} // namespace linglong
