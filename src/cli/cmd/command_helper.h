/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang <yuanqiliang@uniontech.com>
 *
 * Maintainer: yuanqiliang <yuanqiliang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QtGlobal>
#include <QDebug>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <QFile>
#include <sys/wait.h>
#include <grp.h>
#include <stdlib.h>
#include <QStringList>
#include <QJsonArray>

#include "module/util/singleton.h"
#include "module/runtime/container.h"

namespace linglong {
namespace cli {

class CommandHelper
    : public QObject
    , public linglong::util::Singleton<CommandHelper>
{
    Q_OBJECT
    friend class linglong::util::Singleton<CommandHelper>;

public:
    void showContainer(const ContainerList &list, const QString &format);
    int namespaceEnter(pid_t pid, const QStringList &args);
    QStringList getUserEnv(const QStringList &envList);

private:
    CommandHelper(/* args */) = default;
    ~CommandHelper() = default;
    inline int bringDownPermissionsTo(const struct stat &fileStat);
    int execArgs(const std::vector<std::string> &args, const std::vector<std::string> &envStrVector);
    QList<pid_t> childrenOf(pid_t p);
};

inline int CommandHelper::bringDownPermissionsTo(const struct stat &fileStat)
{
    __gid_t newGid[1] = {fileStat.st_gid};

    setgroups(1, newGid);

    setgid(fileStat.st_gid);
    setegid(fileStat.st_gid);

    setuid(fileStat.st_uid);
    seteuid(fileStat.st_uid);
    return 0;
}

} // namespace cli
} // namespace linglong

#define COMMAND_HELPER linglong::cli::CommandHelper::instance()
