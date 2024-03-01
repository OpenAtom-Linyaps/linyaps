/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_

#include <gmock/gmock.h>

#include "linglong/service/app_manager.h"
#include "linglong/utils/error/error.h"

#include <QVector>

#include <iostream>

namespace linglong::cli::test {

class MockAppManager : public service::AppManager
{
private:
    repo::Repo *nullRepo = nullptr;
    ocppi::cli::CLI *nullCli = nullptr;

public:
    MockAppManager()
        : service::AppManager(*nullRepo, *nullCli)
    {
    }

    MOCK_METHOD(linglong::utils::error::Result<void>,
                Run,
                (const linglong::service::RunParamOption &paramOption),
                (override));

    MOCK_METHOD(linglong::service::Reply,
                Exec,
                (const linglong::service::ExecParamOption &paramOption),
                (override));

    MOCK_METHOD(linglong::service::QueryReply, ListContainer, (), (override));

    MOCK_METHOD(linglong::service::Reply, Stop, (const QString &ContainerID), (override));
};

} // namespace linglong::cli::test

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_
