/*
 * Copyright (c) 2022. ${ORGANIZATION_NAME}. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_
#define LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_

#include "oci.h"
#include "app.h"

namespace linglong {
namespace runtime {

// TODO: move to runtime.cpp
inline void registerAllMetaType()
{
    registerAllOciMetaType();

    qJsonRegister<App>();
    qJsonRegister<Layer>();
    qJsonRegister<Permission>();

    qJsonRegister<Container>();
}

} // namespace runtime
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_RUNTIME_RUNTIME_H_
