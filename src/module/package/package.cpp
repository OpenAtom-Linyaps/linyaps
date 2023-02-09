/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package.h"

#include "info.h"

#include <QMetaType>

void linglong::package::registerAllMetaType()
{
    qJsonRegister<linglong::package::Info>();
    qJsonRegister<linglong::package::Permission>();
    qJsonRegister<linglong::package::Filesystem>();
    qJsonRegister<linglong::package::User>();
    qJsonRegister<linglong::package::OverlayfsRootfs>();
    qJsonRegister<linglong::package::AppMetaInfo>();
}
