/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_COMMAND_HELPER_H_
#define LINGLONG_SRC_MODULE_UTIL_COMMAND_HELPER_H_

#include "module/runtime/container.h"
#include "module/util/singleton.h"

namespace linglong {
namespace util {

class CommandHelper : public QObject, public linglong::util::Singleton<CommandHelper>
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
    int execArgs(const std::vector<std::string> &args,
                 const std::vector<std::string> &envStrVector);
    QList<pid_t> childrenOf(pid_t p);
};

} // namespace util
} // namespace linglong

#define COMMAND_HELPER linglong::util::CommandHelper::instance()

#endif
