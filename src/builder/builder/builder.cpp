/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "builder.h"

#include "project.h"

void linglong::builder::registerAllMetaType()
{
    qJsonRegister<linglong::builder::BuildDepend>();
    qJsonRegister<linglong::builder::Project>();
    qJsonRegister<linglong::builder::Variables>();
    qJsonRegister<linglong::builder::Package>();
    qJsonRegister<linglong::builder::BuilderRuntime>();
    qJsonRegister<linglong::builder::Source>();
    qJsonRegister<linglong::builder::Build>();
    qJsonRegister<linglong::builder::BuildManual>();
    qJsonRegister<linglong::builder::Enviroment>();
}
