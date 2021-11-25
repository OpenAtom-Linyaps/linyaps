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
    Q_JSON_PROPERTY(QString, refID);
};
Q_JSON_DECLARE_PTR_METATYPE(Layer)

/*!
 * AppBase: base for run, you can use full run or let it empty
 */
class AppBase : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(AppBase)
    Q_JSON_PROPERTY(QStringList, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(AppBase)

class AppPrivate;
class App : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_PTR_PROPERTY(Layer, package);
    Q_JSON_PTR_PROPERTY(Layer, runtime);
    //    TODO: should config base mount point
    //    Q_JSON_PTR_PROPERTY(AppBase, base);

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    static App *load(const QString &configFilepath);

    Container *container() const;

    int start();

private:
    QScopedPointer<AppPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), App)
};
Q_JSON_DECLARE_PTR_METATYPE(App)