/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_COMMAND_HELPER_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_COMMAND_HELPER_H_

#include "linglong/util/command_helper.h"

class MockCommandHelper : public linglong::util::CommandHelper
{
public:
    ~MockCommandHelper() = default;

    inline void showContainer(const QList<QSharedPointer<Container>> &list,
                              const QString &format) override
    {
    }

    inline int namespaceEnter(pid_t pid, const QStringList &args) override { return 0; }

    inline QStringList getUserEnv(const QStringList &envList) override
    {
        return QStringList{ "1" };
    }
};

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_COMMAND_HELPER_H_