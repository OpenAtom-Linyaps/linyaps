/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_API_DBUS_V1_MOCK_PACKAGE_MANAGER_H_
#define LINGLONG_API_DBUS_V1_MOCK_PACKAGE_MANAGER_H_

#include <gmock/gmock.h>

#include "linglong/api/dbus/v1/package_manager.h"

namespace linglong::api::dbus::v1::test {

class MockPackageManager : public PackageManager
{
    using PackageManager::PackageManager;

public:
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Install,
                (linglong::service::InstallParamOption installParamOption),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                ModifyRepo,
                (const QString &name, const QString &url),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::QueryReply>,
                Query,
                (linglong::service::QueryParamOption paramOption),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Uninstall,
                (linglong::service::UninstallParamOption paramOption),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::Reply>,
                Update,
                (linglong::service::ParamOption paramOption),
                (override));
    MOCK_METHOD(QDBusPendingReply<linglong::service::QueryReply>, getRepoInfo, (), (override));
};
} // namespace linglong::api::dbus::v1::test

#endif
