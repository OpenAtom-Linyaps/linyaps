/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
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

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QScopedPointer>

#include <DSingleton>



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

protected:
    UapManager();
    ~UapManager() override;

private:
    QScopedPointer<UapManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), UapManager)

    
};