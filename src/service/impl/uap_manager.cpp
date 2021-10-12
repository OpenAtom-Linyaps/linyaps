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

#include "module/repo/repohelper.h"
#include "uap_manager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <module/runtime/app.h>
#include <module/util/fs.h>
#include <QJsonArray>

#include "job_manager.h"
#include "module/util/httpclient.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;

class UapManagerPrivate
{
public:
    explicit UapManagerPrivate(UapManager *parent)
        : q_ptr(parent)
    {
    }

    UapManager *q_ptr = nullptr;
};

UapManager::UapManager()
    : dd_ptr(new UapManagerPrivate(this))
{
}

UapManager::~UapManager() = default;

bool UapManager::BuildUap(const QString Config, const QString DataDir, const QString UapPath)
{
    Q_D(UapManager);
    Package create_package;
    create_package.InitUap(Config, DataDir);
    return create_package.MakeUap(UapPath);
}

bool UapManager::BuildOuap(const QString UapPath, const QString RepoPath, const QString OuapPath)
{
    Q_D(UapManager);
    Package create_package;
    return create_package.MakeOuap(UapPath, RepoPath, OuapPath);
}

bool UapManager::Extract(const QString UapPath, const QString ExtractDir)
{
    Q_D(UapManager);
    Package create_package;
    return create_package.Extract(UapPath, ExtractDir);
}

bool UapManager::Check(const QString UapExtractDir)
{
    Q_D(UapManager);
    Package create_package;
    return create_package.Check(UapExtractDir);
}

bool UapManager::GetInfo(const QString Uappath, const QString InfoPath)
{
    Q_D(UapManager);
    Package create_package;
    return create_package.GetInfo(Uappath, InfoPath);
}
