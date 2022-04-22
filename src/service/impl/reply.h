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
} // namespace service
} // namespace linglong

Q_DECLARE_METATYPE(linglong::service::Reply)