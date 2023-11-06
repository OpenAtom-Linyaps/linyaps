/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_API_DBUS_V1_MOCK_APP_MANAGER_H_
#define LINGLONG_API_DBUS_V1_MOCK_APP_MANAGER_H_

#include <gmock/gmock.h>

#include "linglong/api/dbus/v1/app_manager.h"

namespace linglong::api::dbus::v1::test {
class MockAppManager : public AppManager
{
    using AppManager::AppManager;

public:
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Exec,
                (linglong::service::ExecParamOption paramOption),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::QueryReply>, ListContainer, (), (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Start,
                (linglong::service::RunParamOption paramOption),
                (override));

    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Stop,
                (const QString &ContainerID),
                (override));
};
} // namespace linglong::api::dbus::v1::test

#endif
