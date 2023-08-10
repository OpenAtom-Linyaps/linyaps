/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_REPLY_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_REPLY_H_

#include <QDBusArgument>

namespace linglong {
namespace service {

/**
 * @brief DBus方法调用的应答
 */
class Reply
{
public:
    int code;        ///< 状态码
    QString message; ///< 状态码对应的消息
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const Reply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.message;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Reply &reply)
{
    argument.beginStructure();
    argument >> reply.code >> reply.message;
    argument.endStructure();
    return argument;
}

class QueryReply : public Reply
{
public:
    // 查询结果为一个json字符串 失败场景为空串
    QString result;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const QueryReply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.message << reply.result;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, QueryReply &reply)
{
    argument.beginStructure();
    argument >> reply.code >> reply.message >> reply.result;
    argument.endStructure();
    return argument;
}

} // namespace service
} // namespace linglong

Q_DECLARE_METATYPE(linglong::service::Reply)
Q_DECLARE_METATYPE(linglong::service::QueryReply)
#endif
