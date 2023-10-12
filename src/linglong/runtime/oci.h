/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_OCI_H_
#define LINGLONG_SRC_MODULE_RUNTIME_OCI_H_

#include "linglong/util/qserializer/deprecated.h"

class Mount : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Mount)
    Q_JSON_PROPERTY(QString, destination);
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QStringList, options);
};
Q_JSON_DECLARE_PTR_METATYPE(Mount)

#endif
