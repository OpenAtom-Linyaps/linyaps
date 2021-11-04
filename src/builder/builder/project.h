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

class Project : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Project)
public:
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, id);
};

} // namespace builder
} // namespace linglong

using linglong::builder::Project;

Q_JSON_DECLARE_PTR_METATYPE(Project)