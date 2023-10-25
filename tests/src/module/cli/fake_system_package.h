/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_SYSTEM_PACKAGE_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_SYSTEM_PACKAGE_H_

#include "dbus_reply.h"
#include "linglong/api/v1/dbus/package_manager1.h"
#include "linglong/dbus_ipc/reply.h"
#include "linglong/util/status_code.h"

class MockSystemPackage : public OrgDeepinLinglongPackageManager1Interface
{

public:
    using OrgDeepinLinglongPackageManager1Interface::OrgDeepinLinglongPackageManager1Interface;

public: // METHODS
    inline QDBusPendingReply<linglong::service::Reply>
    GetDownloadStatus(linglong::service::ParamOption paramOption, int type) override
    {
        linglong::service::Reply r;
        r.code = STATUS_CODE(kPkgInstallSuccess);
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::Reply>
    Install(linglong::service::InstallParamOption installParamOption) override
    {
        linglong::service::Reply r;
        r.code = 0;
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::Reply> ModifyRepo(const QString &name,
                                                                  const QString &url) override
    {
        linglong::service::Reply r;
        r.code = 0;
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::QueryReply>
    Query(linglong::service::QueryParamOption paramOption) override
    {
        linglong::service::QueryReply r;
        r.code = STATUS_CODE(kErrorPkgQuerySuccess);
        r.message = "";
        r.result=R"([ {
            "appId": "xxx",
            "name": "xxx",
            "version": "xxx",
            "arch": "xxx",
            "kind": "xxx",
            "runtime": "xxx",
            "repoName": "xxx",
            "description": "xxx",
            "user": "xxx",
            "size": "xxx",
            "channel": "xxx",
            "module": "xxx"
        } ])";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::Reply>
    Uninstall(linglong::service::UninstallParamOption paramOption) override
    {
        linglong::service::Reply r;
        r.code = STATUS_CODE(kPkgUninstallSuccess);
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::Reply>
    Update(linglong::service::ParamOption paramOption) override
    {
        linglong::service::Reply r;
        r.code = STATUS_CODE(kErrorPkgUpdateSuccess);
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline QDBusPendingReply<linglong::service::QueryReply> getRepoInfo() override
    {
        linglong::service::QueryReply r;
        r.code = 0;
        r.message = "";
        return createReply("", "", "", "", r);
    }

    inline void setTimeout(int timeout) { }
};

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_SYSTEM_PACKAGE_H_
