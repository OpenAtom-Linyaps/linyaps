/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>

#include "module/util/json.h"

namespace linglong {
namespace builder {

class Variables : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Variables)
public:
    Q_JSON_PROPERTY(QString, id);
};

class Project : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Project)
public:
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PTR_PROPERTY(Variables, variables);
};

} // namespace builder
} // namespace linglong

using linglong::builder::Project;
using linglong::builder::Variables;

Q_JSON_DECLARE_PTR_METATYPE(Project)
Q_JSON_DECLARE_PTR_METATYPE(Variables)