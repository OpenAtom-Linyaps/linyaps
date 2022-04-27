/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang@uniontech.com
 *
 * Maintainer: yuanqiliang@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDBusArgument>

namespace linglong {
namespace service {
class Reply
{
public:
    int code;
    QString message;
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