/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_
#define LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_

#include "app.h"
#include "app_config.h"
#include "oci.h"

namespace linglong {
namespace runtime {

// TODO: move to runtime.cpp
inline void registerAllMetaType()
{
    registerAllOciMetaType();

    qJsonRegister<Layer>();
    qJsonRegister<MountYaml>();
    qJsonRegister<AppPermission>();
    qJsonRegister<App>();
    qJsonRegister<Container>();
    qJsonRegister<AppConfig>();
}

} // namespace runtime
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_
