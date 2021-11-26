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

#include "src/module/util/yaml.h"
#include "src/module/runtime/oci.h"

class TestApp : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(TestApp)
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QStringList, mounts);
    Q_JSON_PROPERTY(NamespaceList, namespaces);
    Q_JSON_PTR_PROPERTY(Root, root);
};
Q_JSON_DECLARE_PTR_METATYPE(TestApp)