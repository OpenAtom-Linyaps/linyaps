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

#include "oci.h"
#include "container.h"

class Layer : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Layer)
    Q_JSON_PROPERTY(QString, ref);
};
Q_JSON_DECLARE_PTR_METATYPE(Layer)

/*!
 * Permission: base for run, you can use full run or let it empty
 */
class Permission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Permission)
    Q_JSON_PROPERTY(QStringList, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(Permission)

class AppPrivate;
class App : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_PTR_PROPERTY(Layer, package);
    Q_JSON_PTR_PROPERTY(Layer, runtime);

    // TODO: should config base mount point
    Q_JSON_PTR_PROPERTY(Permission, permission);

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    static App *load(const QString &configFilepath, const QString &desktopExec, bool useFlatpakRuntime);

    Container *container() const;

    int start();

private:
    QScopedPointer<AppPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), App)
};
Q_JSON_DECLARE_PTR_METATYPE(App)