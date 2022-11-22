/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_APP_CONFIG_H_
#define LINGLONG_SRC_MODULE_RUNTIME_APP_CONFIG_H_

#include "module/util/serialize/json.h"

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
#endif