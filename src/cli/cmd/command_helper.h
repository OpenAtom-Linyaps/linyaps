/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_CMD_COMMAND_HELPER_H_
#define LINGLONG_SRC_CLI_CMD_COMMAND_HELPER_H_

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

} // namespace cli
} // namespace linglong
#endif

#define COMMAND_HELPER linglong::cli::CommandHelper::instance()
