/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_INFO_H_
#define LINGLONG_SRC_MODULE_PACKAGE_INFO_H_

#include "linglong/runtime/oci.h"

#include <QDBusArgument>
#include <QList>
#include <QObject>

namespace linglong {
namespace package {

/**
 * @brief User The Info class
 *
 * 包信息类
 */
class User : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(User)
    Q_JSON_PROPERTY(QString, desktop);
    Q_JSON_PROPERTY(QString, documents);
    Q_JSON_PROPERTY(QString, downloads);
    Q_JSON_PROPERTY(QString, music);
    Q_JSON_PROPERTY(QString, pictures);
    Q_JSON_PROPERTY(QString, videos);
    Q_JSON_PROPERTY(QString, templates);
    Q_JSON_PROPERTY(QString, temp);
    Q_JSON_ITEM_MEMBER(QString, public_share, publicShare);
};

/*!
 * \brief The Info class
 * \details 文件系统挂载权限信息
 */
class Filesystem : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Filesystem)
    Q_JSON_PROPERTY(QSharedPointer<User>, user);
};

/*!
 * \brief The Info class
 * \details 权限信息类
 */
class Permission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Permission)
    Q_JSON_PROPERTY(bool, autostart);
    Q_JSON_PROPERTY(bool, notification);
    Q_JSON_PROPERTY(bool, trayicon);
    Q_JSON_PROPERTY(bool, clipboard);
    Q_JSON_PROPERTY(bool, account);
    Q_JSON_PROPERTY(bool, bluetooth);
    Q_JSON_PROPERTY(bool, camera);
    Q_JSON_ITEM_MEMBER(bool, audio_record, audioRecord);
    Q_JSON_ITEM_MEMBER(bool, installed_apps, installedApps);
    Q_JSON_PTR_PROPERTY(Filesystem, filesystem);
};

class OverlayfsRootfs : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(OverlayfsRootfs)
    Q_JSON_PROPERTY(QList<QSharedPointer<Mount>>, mounts);
};

/*!
 * Info is the data of /opt/apps/{package-id}/info.json. The spec can get from here:
 * https://doc.chinauos.com/content/M7kCi3QB_uwzIp6HyF5J
 */
class Info : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Info)

public:
    Q_JSON_PROPERTY(QString, appid);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QStringList, arch);
    Q_JSON_PROPERTY(QString, kind);
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, module);
    Q_JSON_PROPERTY(quint64, size);
    Q_JSON_PROPERTY(QString, description);

    // ref of runtime
    Q_JSON_PROPERTY(QString, runtime);
    Q_JSON_PROPERTY(QString, base);

    // permissions
    Q_JSON_PTR_PROPERTY(Permission, permissions);

    // overlayfs mount
    Q_JSON_PTR_PROPERTY(OverlayfsRootfs, overlayfs);
};

} // namespace package
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, Info)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, Permission)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, Filesystem)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, User)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, OverlayfsRootfs)

#endif /* LINGLONG_SRC_MODULE_PACKAGE_INFO_H_ */
