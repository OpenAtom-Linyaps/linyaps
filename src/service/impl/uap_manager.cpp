/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
#include "module/repo/repohelper.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;
using linglong::dbus::RetCode;

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

RetMessageList UapManager::Install(const QStringList UapFileList)
{
    Q_D(UapManager);
    //创建返回值对象
    RetMessageList list;
    list.clear();
    for (auto it : UapFileList) {
        auto c = QPointer<RetMessage>(new RetMessage);
        Package *pkg = new Package;
        //复制ret_QString uap包
        c->message = QFileInfo(it).fileName();

        if (!it.endsWith(".uap")) {
            qInfo() << it + QString(" isn't a uap package!!!");
            delete pkg;
            pkg = nullptr;
            c->state = false;
            c->code = RetCode(RetCode::uap_name_format_error);
            continue;
        }
        if (!fileExists(it)) {
            qInfo() << it + QString(" don't exists!");
            delete pkg;
            pkg = nullptr;
            c->state = false;
            c->code = RetCode(RetCode::uap_file_not_exists);
            continue;
        }

        if (!pkg->InitUapFromFile(it)) {
            qInfo() << "install " + it + " failed!!!";
            c->state = false;
            c->code = RetCode(RetCode::uap_install_failed);
            list.push_back(c);
            delete pkg;
            pkg = nullptr;
        } else {
            qInfo() << "install " + it + " successed!!!";
            c->state = true;
            c->code = RetCode(RetCode::uap_install_success);
            list.push_back(c);
            delete pkg;
            pkg = nullptr;
        }
    }
    return list;
}