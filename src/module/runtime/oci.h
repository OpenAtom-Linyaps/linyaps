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

#include "module/util/json.h"

class Root : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Root)
public:
    Q_JSON_PROPERTY(QString, path);
    Q_JSON_PROPERTY(bool, readonly);
};
Q_JSON_DECLARE_PTR_METATYPE(Root)

class Namespace : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Namespace)
    Q_JSON_PROPERTY(QString, type);
};
Q_JSON_DECLARE_PTR_METATYPE(Namespace)

class IDMap : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(IDMap)
    Q_JSON_PROPERTY(quint64, hostID);
    Q_JSON_PROPERTY(quint64, containerID);
    Q_JSON_PROPERTY(quint64, size);
};
Q_JSON_DECLARE_PTR_METATYPE(IDMap)

class Linux : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Linux)
    Q_JSON_PROPERTY(NamespaceList, namespaces);
    Q_JSON_PROPERTY(IDMapList, uidMappings);
    Q_JSON_PROPERTY(IDMapList, gidMappings);
};
Q_JSON_DECLARE_PTR_METATYPE(Linux)

class Process : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Process)
    Q_JSON_PROPERTY(QStringList, args);
    Q_JSON_PROPERTY(QStringList, env);
    Q_JSON_PROPERTY(QString, cwd);
};
Q_JSON_DECLARE_PTR_METATYPE(Process)

class Mount : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Mount)
    Q_JSON_PROPERTY(QString, destination);
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QStringList, options);

private:
};
Q_JSON_DECLARE_PTR_METATYPE(Mount)

class Hook : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Hook)
    Q_JSON_PROPERTY(QString, path);
    Q_JSON_PROPERTY(QStringList, args);
    Q_JSON_PROPERTY(QStringList, env);
};
Q_JSON_DECLARE_PTR_METATYPE(Hook)

class Hooks : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Hooks)
    Q_JSON_PROPERTY(HookList, prestart);
    Q_JSON_PROPERTY(HookList, poststart);
    Q_JSON_PROPERTY(HookList, poststop);
};
Q_JSON_DECLARE_PTR_METATYPE(Hooks)

#undef linux
class Runtime : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Runtime)
    Q_JSON_PROPERTY(QString, ociVersion);
    Q_JSON_PTR_PROPERTY(Root, root);
    Q_JSON_PTR_PROPERTY(Process, process);
    Q_JSON_PROPERTY(QString, hostname);
    Q_JSON_PROPERTY(MountList, mounts);
    Q_JSON_PTR_PROPERTY(Linux, linux);
    Q_JSON_PTR_PROPERTY(Hooks, hooks);
};
Q_JSON_DECLARE_PTR_METATYPE(Runtime)

inline void ociJsonRegister()
{
    qJsonRegister<Root>();
    qJsonRegister<Linux>();
    qJsonRegister<Mount>();
    qJsonRegister<Namespace>();
    qJsonRegister<Hook>();
    qJsonRegister<Runtime>();
    qJsonRegister<Process>();
    qJsonRegister<IDMap>();
}