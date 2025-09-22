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
    std::function<linglong::utils::error::Result<QString>(const QStringList &)> wrapExecFunc;

    linglong::utils::error::Result<QString> exec(const QStringList &args) noexcept override
    {
        return wrapExecFunc ? wrapExecFunc(args) : Cmd::exec(args);
    }

    // mock exists
    std::function<linglong::utils::error::Result<bool>()> wrapExistsFunc;

    linglong::utils::error::Result<bool> exists() noexcept override
    {
        return wrapExistsFunc ? wrapExistsFunc() : Cmd::exists();
    }

    // mock setEnv
    std::function<linglong::utils::command::Cmd &(const QString &, const QString &)> wrapSetEnvFunc;

    Cmd &setEnv(const QString &name, const QString &value) noexcept override
    {
        return wrapSetEnvFunc ? wrapSetEnvFunc(name, value) : Cmd::setEnv(name, value);
    }
};
