/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_REPLY_H_
#define LINGLONG_TEST_MODULE_CLI_REPLY_H_

#include <qdbusmetatype.h>
#define private public
#include <private/qdbusmessage_p.h>
#undef private

#include <QDBusPendingReply>

template<typename ReturnValue>
QDBusPendingReply<ReturnValue> createReply(const ReturnValue &value,
                                           const QString &dest = "",
                                           const QString &path = "",
                                           const QString &interface = "",
                                           const QString &method = "")
{
    auto call = QDBusMessage::createMethodCall(dest, path, interface, method);
    auto msg = call.createReply();
    msg << QVariant::fromValue(value);
    msg.d_ptr->signature = QDBusMetaType::typeToSignature(QMetaType::fromType<ReturnValue>().id());
    return QDBusPendingCall::fromCompletedCall(msg);
}

#endif // LINGLONG_TEST_MODULE_CLI_REPLY_H_
