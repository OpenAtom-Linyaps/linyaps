/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_PACKAGE_MANAGER_H_
#define LINGLONG_TEST_MODULE_CLI_PACKAGE_MANAGER_H_

#include "linglong/dbus_ipc/reply.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/util/status_code.h"

using namespace linglong::service;

class MockPackageManager : public PackageManager

{
public:
    ~MockPackageManager() = default;

    inline auto ModifyRepo(const QString &name, const QString &url) -> Reply override
    {
        Reply reply;
        reply.code = 0;
        reply.message = "";
        return reply;
    }

    inline auto Query(const QueryParamOption &paramOption) -> QueryReply override
    {
        linglong::service::QueryReply queryReply;
        queryReply.code = 0;
        queryReply.message = "";
        return queryReply;
    }

    auto Install(const InstallParamOption &installParamOption) -> Reply override
    {
        Reply reply;
        reply.code = 0;
        reply.message = "";
        return reply;
    }

    auto Uninstall(const UninstallParamOption &paramOption) -> Reply override
    {
        Reply reply;
        reply.code = STATUS_CODE(kPkgUninstallSuccess);
        reply.message = "";
        return reply;
    }

    inline void setNoDBusMode(bool enable) override { }
};

#endif // LINGLONG_TEST_MODULE_CLI_PACKAGE_MANAGER_H_