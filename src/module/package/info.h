/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_MODULE_PACKAGE_INFO_H_
#define LINGLONG_BOX_SRC_MODULE_PACKAGE_INFO_H_

#include <QDBusArgument>
#include <QList>
#include <QObject>

#include "module/util/json.h"
#include "ref.h"

namespace linglong {
namespace package {

/*!
 * Info is the data of /opt/apps/{package-id}/info.json. The spec can get from here:
 * https://doc.chinauos.com/content/M7kCi3QB_uwzIp6HyF5J
 */
class Info : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Info)

public:
    Q_JSON_PROPERTY(QString, appid);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QStringList, arch);
    Q_JSON_PROPERTY(QString, kind);
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, description);

    // ref of runtime
    Q_JSON_PROPERTY(QString, runtime);
    Q_JSON_PROPERTY(QString, base);
};

} // namespace package
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, Info)

// inline QDBusArgument &operator<<(QDBusArgument &argument, const linglong::package::Info &message)
//{
//     argument.beginStructure();
//     argument << message.appid;
//     argument << message.name;
//     argument.endStructure();
//     return argument;
// }
//
// inline const QDBusArgument &operator>>(const QDBusArgument &argument, linglong::package::Info &message)
//{
//     argument.beginStructure();
//     argument >> message.appid;
//     argument >> message.name;
//     argument.endStructure();
//     return argument;
// }

#endif /* LINGLONG_BOX_SRC_MODULE_PACKAGE_INFO_H_ */
