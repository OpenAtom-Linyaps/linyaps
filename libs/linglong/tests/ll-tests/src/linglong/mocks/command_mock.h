// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "linglong/utils/command/cmd.h"
#include "linglong/utils/error/error.h"

#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

class MockCommand : public linglong::utils::command::Cmd
{
public:
    MockCommand(const QString &command)
        : Cmd(command)
    {
    }

    // mock exec
    std::function<linglong::utils::error::Result<QString>(const QStringList &)> warpExecFunc;

    linglong::utils::error::Result<QString> exec(const QStringList &args) noexcept override
    {
        return warpExecFunc ? warpExecFunc(args) : Cmd::exec(args);
    }

    // mock exists
    std::function<linglong::utils::error::Result<bool>()> warpExistsFunc;

    linglong::utils::error::Result<bool> exists() noexcept override
    {
        return warpExistsFunc ? warpExistsFunc() : Cmd::exists();
    }

    // mock setEnv
    std::function<linglong::utils::command::Cmd &(const QString &, const QString &)> warpSetEnvFunc;

    Cmd &setEnv(const QString &name, const QString &value) noexcept override
    {
        return warpSetEnvFunc ? warpSetEnvFunc(name, value) : Cmd::setEnv(name, value);
    }
};
