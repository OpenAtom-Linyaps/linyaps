/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QFileInfo>
#include <DSingleton>

#include "qdbus_retmsg.h"
#include "module/package/package.h"
#include "dbus_retcode.h"

class UapManagerPrivate;
class UapManager : public QObject
    , protected QDBusContext
    , public Dtk::Core::DSingleton<UapManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.UapManager")

    friend class Dtk::Core::DSingleton<UapManager>;

public Q_SLOTS:
    bool BuildUap(const QString Config, const QString DataDir,const QString UapPath);
    bool BuildOuap(const QString UapPath,const QString RepoPath,const QString OuapPath);
    bool Extract(const QString UapPath,const QString ExtractDir);
    bool Check(const QString UapExtractDir);
    bool GetInfo(const QString Uappath,const QString InfoPath);
    RetMessageList Install(const QStringList UapFileList);

protected:
    UapManager();
    ~UapManager() override;

private:
    QScopedPointer<UapManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), UapManager)

    
};