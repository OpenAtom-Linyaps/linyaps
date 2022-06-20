/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QStringList>
#include <QString>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QObject>
#include <QProcess>
#include <stdlib.h>
#include <iostream>

#include "file.h"
#include "runner.h"

namespace linglong {
namespace repo {
/*!
 * ostree仓库制作
 * @param dest_path 仓库路径
 * @param mode      仓库模式
 * @return
 */
bool inline makeOstree(const QString &dest_path, const QString &mode)
{
    if (!linglong::util::dirExists(dest_path)) {
        linglong::util::createDir(dest_path);
    }
    // QString program = "ostree";
    // QStringList arguments;

    // arguments << QString("--repo=") + dest_path << QString("init") << QString("--mode=") + mode;

    auto ret = linglong::runner::Runner("ostree", {"--repo=" + dest_path, "init", "--mode=" + mode}, 3000);

    return ret;
}

} // namespace repo
} // namespace linglong
