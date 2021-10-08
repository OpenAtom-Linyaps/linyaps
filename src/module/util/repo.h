/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

using namespace linglong::util;

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
    QString program = "ostree";
    QStringList arguments;
    arguments << QString("--repo=") + dest_path << QString("init") << QString("--mode=") + mode;
    QProcess *myProcess = new QProcess();
    myProcess->start(program, arguments);
    if (!myProcess->waitForStarted()) {
        qInfo() << "repo init failed!!!";
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    if (!myProcess->waitForFinished()) {
        qInfo() << "repo init failed!!!";
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    auto ret_code = myProcess->exitStatus();
    if (ret_code != 0) {
        qInfo() << "run failed: " << ret_code;
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    delete myProcess;
    myProcess = nullptr;
    qInfo() << "make ostree of " + dest_path + "successed!!!";
    return true;
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
bool inline commitOstree(const QString &repo_path, const QString &summary, const QString &body, const QString &branch, const QString &dir_data, QString &out_commit)
{
    if (!dirExists(dir_data)) {
        qInfo() << "datadir not exists!!!";
        return false;
    }
    if (!dirExists(repo_path)) {
        qInfo() << "repo_path not exists!!!";
        return false;
    }
    QString program = "ostree";
    QStringList arguments;
    arguments << QString("--repo=") + repo_path << QString("commit") << QString("-s") << summary << QString("-m") << body << QString("-b") << branch << QString("--tree=dir=") + dir_data;
    QProcess *myProcess = new QProcess();
    myProcess->start(program, arguments);
    if (!myProcess->waitForStarted()) {
        qInfo() << "repo commit failed!!!";
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    if (!myProcess->waitForFinished()) {
        qInfo() << "repo commit failed!!!";
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    auto ret_code = myProcess->exitStatus();
    if (ret_code != 0) {
        qInfo() << "run failed: " << ret_code;
        delete myProcess;
        myProcess = nullptr;
        return false;
    }
    out_commit = QString::fromLocal8Bit(myProcess->readAllStandardOutput());
    out_commit = out_commit.simplified();

    qInfo() << "commit 值：" + out_commit;
    qInfo() << "commit successed!!!";

    delete myProcess;
    myProcess = nullptr;
    return true;
}

} // namespace repo
} // namespace linglong
