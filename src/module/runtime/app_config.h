/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/util/json.h"

namespace linglong {
namespace runtime {

class AppConfig : public Serialize
{
    Q_OBJECT
    Q_SERIALIZE_CONSTRUCTOR(AppConfig)
public:
    Q_SERIALIZE_PROPERTY(QStringList, appMountDevList);

};

} // namespace runtime
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, AppConfig)