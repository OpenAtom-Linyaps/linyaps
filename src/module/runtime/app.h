/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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