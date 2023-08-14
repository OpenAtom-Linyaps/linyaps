/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_OCI_H_
#define LINGLONG_SRC_MODULE_RUNTIME_OCI_H_

#include "linglong/util/qserializer/deprecated.h"

class Root : public JsonSerialize
{
    Q_OBJECT;

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
    Q_JSON_ITEM_MEMBER(quint64, hostID, hostId);
    Q_JSON_ITEM_MEMBER(quint64, containerID, containerId);
    Q_JSON_PROPERTY(quint64, size);
};
Q_JSON_DECLARE_PTR_METATYPE(IdMap)

class Linux : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Linux)
    Q_JSON_PROPERTY(QList<QSharedPointer<Namespace>>, namespaces);
    Q_JSON_PROPERTY(QList<QSharedPointer<IdMap>>, uidMappings);
    Q_JSON_PROPERTY(QList<QSharedPointer<IdMap>>, gidMappings);
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
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Mount)
    Q_JSON_PROPERTY(QString, destination);
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QStringList, options);
};
Q_JSON_DECLARE_PTR_METATYPE(Mount)

class Hook : public JsonSerialize
{
    Q_OBJECT;
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
    Q_JSON_PROPERTY(QList<QSharedPointer<Hook>>, prestart);
    Q_JSON_PROPERTY(QList<QSharedPointer<Hook>>, poststart);
    Q_JSON_PROPERTY(QList<QSharedPointer<Hook>>, poststop);
};
Q_JSON_DECLARE_PTR_METATYPE(Hooks)

class AnnotationsOverlayfsRootfs : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AnnotationsOverlayfsRootfs)
    Q_JSON_PROPERTY(QString, lowerParent);
    Q_JSON_PROPERTY(QString, upper);
    Q_JSON_PROPERTY(QString, workdir);
    Q_JSON_PROPERTY(QList<QSharedPointer<Mount>>, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(AnnotationsOverlayfsRootfs)

class AnnotationsNativeRootfs : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AnnotationsNativeRootfs)
    Q_JSON_PROPERTY(QList<QSharedPointer<Mount>>, mounts);
};
Q_JSON_DECLARE_PTR_METATYPE(AnnotationsNativeRootfs)

class DBusProxy : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(DBusProxy)
    Q_JSON_PROPERTY(bool, enable);
    Q_JSON_PROPERTY(QString, busType);
    Q_JSON_ITEM_MEMBER(QString, appID, appId);
    Q_JSON_PROPERTY(QString, proxyPath);
    Q_JSON_PROPERTY(QStringList, name);
    Q_JSON_PROPERTY(QStringList, path);
    Q_JSON_PROPERTY(QStringList, interface);
};
Q_JSON_DECLARE_PTR_METATYPE(DBusProxy)

class Annotations : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Annotations)
    Q_JSON_PROPERTY(QString, containerRootPath);
    Q_JSON_PTR_PROPERTY(AnnotationsOverlayfsRootfs, overlayfs);
    Q_JSON_PTR_PROPERTY(AnnotationsNativeRootfs, native);

    Q_JSON_PTR_PROPERTY(DBusProxy, dbusProxyInfo);
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
    Q_JSON_PROPERTY(QList<QSharedPointer<Mount>>, mounts);
    Q_JSON_PTR_PROPERTY(Linux, linux);
    Q_JSON_PTR_PROPERTY(Hooks, hooks);
    Q_JSON_PTR_PROPERTY(Annotations, annotations);
};

Q_JSON_DECLARE_PTR_METATYPE(Runtime);

#endif
