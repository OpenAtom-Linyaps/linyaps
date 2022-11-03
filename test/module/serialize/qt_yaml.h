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

#include "src/module/util/serialize/yaml.h"
#include "src/module/runtime/oci.h"

namespace linglong {
namespace test {

class MountRule : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(MountRule)
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, options);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QString, destination);
};
D_SERIALIZE_DECLARE(MountRule)

class Permission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Permission)
    // NOTE: custom class type must use with full namespace in qt type system
    Q_JSON_PROPERTY(linglong::test::MountRuleList, mounts);
};
D_SERIALIZE_DECLARE(Permission)

class App : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(App)
    Q_JSON_PROPERTY(QString, version);
    // NOTE: custom class type must use with full namespace in qt type system
    Q_JSON_PTR_PROPERTY(linglong::test::Permission, permissions);
};
D_SERIALIZE_DECLARE(App)

} // namespace test
} // namespace linglong

D_SERIALIZE_REGISTER_TYPE_NM(linglong::test, MountRule)
D_SERIALIZE_REGISTER_TYPE_NM(linglong::test, Permission)
D_SERIALIZE_REGISTER_TYPE_NM(linglong::test, App)

class TestMount : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(TestMount)
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, source);
};
Q_JSON_DECLARE_PTR_METATYPE(TestMount)

class TestPermission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(TestPermission)
    Q_JSON_PROPERTY(TestMountList, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(TestPermission)

class TestApp : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(TestApp)
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QStringList, mounts);
    Q_JSON_PROPERTY(NamespaceList, namespaces);
    Q_JSON_PTR_PROPERTY(Root, root);
    Q_JSON_PTR_PROPERTY(TestPermission, permissions);
};
Q_JSON_DECLARE_PTR_METATYPE(TestApp)
