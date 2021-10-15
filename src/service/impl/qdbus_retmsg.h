/*
* Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
*
* Author:     Iceyer <me@iceyer.net>
*
* Maintainer: Iceyer <me@iceyer.net>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QDBusArgument>
#include <QList>
#include <QObject>
#include "module/util/json.h"

/*!
* RetMessage
* 返回 bool + int + QString的标准内容
*/
class RetMessage : public JsonSerialize
{
   Q_OBJECT;
   Q_JSON_CONSTRUCTOR(RetMessage);

public:
   Q_JSON_PROPERTY(bool , state);
   Q_JSON_PROPERTY(quint32 , code);
   Q_JSON_PROPERTY(QString, message);

};
Q_JSON_DECLARE_PTR_METATYPE(RetMessage)

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const RetMessage &message)
{
    argument.beginStructure();
    argument << message.state;
    argument << message.code;
    argument << message.message;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       RetMessage &message)
{
    argument.beginStructure();
    argument >> message.state;
    argument >> message.code;
    argument >> message.message;
    argument.endStructure();
    return argument;
}
