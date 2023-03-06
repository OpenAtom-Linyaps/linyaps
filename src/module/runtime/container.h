/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_CONTAINER_H_
#define LINGLONG_SRC_MODULE_RUNTIME_CONTAINER_H_

#include "module/util/error.h"
#include "module/util/serialize/json.h"

#include <QDBusArgument>
#include <QList>
#include <QObject>

class Container : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Container)
public:
    Q_JSON_ITEM_MEMBER(QString, ID, id)
    Q_JSON_ITEM_MEMBER(qint64, PID, pid)
    Q_JSON_ITEM_MEMBER(QString, PackageName, packageName)
    Q_JSON_ITEM_MEMBER(QString, WorkingDirectory, workingDirectory)

    linglong::util::Error create(const QString &ref);
};
Q_JSON_DECLARE_PTR_METATYPE(Container)
#endif
