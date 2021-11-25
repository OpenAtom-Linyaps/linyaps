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

#include "fs.h"
#include "runner.h"

using namespace linglong::util;
using namespace linglong::runner;

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
    if (!dirExists(dest_path)) {
        createDir(dest_path);
    }
    // QString program = "ostree";
    // QStringList arguments;

    // arguments << QString("--repo=") + dest_path << QString("init") << QString("--mode=") + mode;

    auto ret = Runner("ostree", {"--repo=" + dest_path, "init", "--mode=" + mode}, 3000);

    return ret;
}

/*!
 * ostree commit
 * @param repo_path     仓库路径
 * @param summary       推送主题
 * @param body          推送概述
 * @param branch        分之名称
 * @param dir_data      数据目录
 * @param out_commit    commit
 * @return
 */
bool inline commitOstree(const QString &repo_path, const QString &summary, const QString &body, const QString &branch,
                         const QString &dir_data, QString &out_commit)
{
    if (!dirExists(dir_data)) {
        qInfo() << "datadir not exists!!!";
        return false;
    }
    if (!dirExists(repo_path)) {
        qInfo() << "repo_path not exists!!!";
        return false;
    }
    bool retval {false};
    QStringList retmsg;

    QString program = "ostree";
    QStringList arguments;
    arguments << QString("--repo=") + repo_path << QString("commit") << QString("-s") << summary << QString("-m")
              << body << QString("-b") << branch << QString("--tree=dir=") + dir_data;

    auto ret = RunnerRet("ostree", arguments, 15 * 60 * 1000);
    std::tie(retval, retmsg) = ret;
    if (retval == false) {
        qInfo() << retmsg;
        return false;
    }
    out_commit = retmsg.first();
    out_commit = out_commit.simplified();

    std::cout << "commit 值：" + out_commit.toStdString() << std::endl;
    qInfo() << "commit successed!!!";

    return true;
}

} // namespace repo
} // namespace linglong
