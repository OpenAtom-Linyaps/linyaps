/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_

#include "dbus_reply.h"
#include "linglong/api/v1/dbus/app_manager1.h"
#include "linglong/api/v1/dbus/gen_org_deepin_linglong_appmanager1.h"
#include "linglong/dbus_ipc/reply.h"
#include "linglong/util/status_code.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariant>

#include <memory>

const QString Dest = "org.deepin.linglong.AppManager";
const QString Path = "/org/deepin/linglong/AppManager";
const QString Interface = "org.deepin.linglong.AppManager";

class MockAppManager : public OrgDeepinLinglongAppManager1Interface
{

    Q_OBJECT
public:
    static inline const char *staticInterfaceName() { return "org.deepin.linglong.AppManager1"; }

public:
    using OrgDeepinLinglongAppManager1Interface::OrgDeepinLinglongAppManager1Interface;

public Q_SLOTS: // METHODS

    inline QDBusPendingReply<linglong::service::Reply>
    Exec(linglong::service::ExecParamOption paramOption) override
    {
        linglong::service::Reply r;
        r.code = 0;
        r.message = "";
        // "org.deepin.linglong.AppManager", "/org/deepin/linglong/AppManager",
        return createReply(Dest, Path, Interface, "Test", r);
    }

    inline QDBusPendingReply<linglong::service::QueryReply> ListContainer() override
    {
        linglong::service::QueryReply r;
        r.code = 0;
        r.message = "";
        return createReply(Dest, Path, Interface, "Test", r);
    }

    inline QDBusPendingReply<linglong::service::Reply>
    Start(linglong::service::RunParamOption paramOption) override
    {
        linglong::service::Reply r;
        r.code = 0;
        r.message = "";
        return createReply(Dest, Path, Interface, "Test", r);
    }

    inline QDBusPendingReply<linglong::service::Reply> Stop(const QString &ContainerID) override
    {
        linglong::service::Reply r;
        r.code = STATUS_CODE(kErrorPkgKillSuccess);
        r.message = "";
        return createReply(Dest, Path, Interface, "Test", r);
    }

Q_SIGNALS: // SIGNALS
};

namespace org {
namespace deepin {
namespace linglong {
typedef ::OrgDeepinLinglongAppManager1Interface AppManager1;
}
} // namespace deepin
} // namespace org

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_APP_MANAGER_H_