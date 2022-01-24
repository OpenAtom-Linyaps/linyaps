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

class IdMap : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(IdMap)
    Q_JSON_PROPERTY(quint64, hostId);
    Q_JSON_PROPERTY(quint64, containerId);
    Q_JSON_PROPERTY(quint64, size);
};
Q_JSON_DECLARE_PTR_METATYPE(IdMap)

class Linux : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Linux)
    Q_JSON_PROPERTY(NamespaceList, namespaces);
    Q_JSON_PROPERTY(IdMapList, uidMappings);
    Q_JSON_PROPERTY(IdMapList, gidMappings);
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

class AnnotationsOverlayfsRootfs : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AnnotationsOverlayfsRootfs)
    Q_JSON_PROPERTY(QString, lower_parent);
    Q_JSON_PROPERTY(QString, upper);
    Q_JSON_PROPERTY(QString, workdir);
    Q_JSON_PROPERTY(MountList, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(AnnotationsOverlayfsRootfs)

class AnnotationsNativeRootfs : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AnnotationsNativeRootfs)
    Q_JSON_PROPERTY(MountList, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(AnnotationsNativeRootfs)

class Annotations : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Annotations)
    Q_JSON_PROPERTY(QString, container_root_path);
    Q_JSON_PTR_PROPERTY(AnnotationsOverlayfsRootfs, overlayfs);
    Q_JSON_PTR_PROPERTY(AnnotationsNativeRootfs, native);
};
Q_JSON_DECLARE_PTR_METATYPE(Annotations)

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
    Q_JSON_PTR_PROPERTY(Annotations, annotations);
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
    qJsonRegister<IdMap>();

    qJsonRegister<Annotations>();
    qJsonRegister<AnnotationsOverlayfsRootfs>();
    qJsonRegister<AnnotationsNativeRootfs>();
}
