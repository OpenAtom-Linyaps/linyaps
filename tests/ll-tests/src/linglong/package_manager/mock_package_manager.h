/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACAKGE_MANAGER_MOCK_PACKAGE_MANAGER_H_
#define LINGLONG_PACAKGE_MANAGER_MOCK_PACKAGE_MANAGER_H_

#include <gmock/gmock.h>

#include "linglong/package_manager/package_manager.h"

namespace linglong::service::test {
class MockPackageManager : public PackageManager
{
    MOCK_METHOD(Reply, ModifyRepo, (const QString &name, const QString &url), (override));
    MOCK_METHOD(Reply, Install, (const InstallParamOption &installParamOption), (override));
    MOCK_METHOD(Reply, Uninstall, (const UninstallParamOption &paramOption), (override));
    MOCK_METHOD(QueryReply, Query, (const QueryParamOption &paramOption), (override));
    MOCK_METHOD(void, setNoDBusMode, (bool enable), (override));
};
} // namespace linglong::service::test

#endif
