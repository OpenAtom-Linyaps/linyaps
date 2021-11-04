/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSysInfo>

#include "job_manager.h"
#include "module/runtime/app.h"
#include "module/util/fs.h"
#include "module/util/singleton.h"
#include "service/impl/dbus_retcode.h"
#include "package_manager.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;
using linglong::dbus::RetCode;

using linglong::service::util::AppInstance;
using linglong::service::util::AppInfo;

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
    {
    }

    QMap<QString, QPointer<App>> apps;

    PackageManager *q_ptr = nullptr;
};

PackageManager::PackageManager()
    : dd_ptr(new PackageManagerPrivate(this)),app_instance_list(linglong::service::util::AppInstance::get())
{
    AppInfo app_info;
    app_info.appid = "org.test.app1";
    app_info.version = "v0.1";
    this->app_instance_list->AppendAppInstance<AppInfo>(app_info);
}

PackageManager::~PackageManager() = default;

/*!
 * 下载软件包
 * @param packageIDList
 */
RetMessageList PackageManager::Download(const QStringList &packageIDList, const QString savePath)
{
    Q_D(PackageManager);

    // return JobManager::instance()->CreateJob([](Job *jr) {
    //     在这里写入真正的实现
    //     QProcess p;
    //     p.setProgram("curl");
    //     p.setArguments({"https://www.baidu.com"});
    //     p.start();
    //     p.waitForStarted();
    //     p.waitForFinished(-1);
    //     qDebug() << p.readAllStandardOutput();
    //     qDebug() << "finish" << p.exitStatus() << p.state();
    // });
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::get();
    return pImpl->Download(packageIDList, savePath);
}

/*!
 * 在线安装软件包
 * @param packageIDList
 */
RetMessageList PackageManager::Install(const QStringList &packageIDList)
{
    Q_D(PackageManager);

    // return JobManager::instance()->CreateJob([](Job *jr) {
    //     // 在这里写入真正的实现
    //     QProcess p;
    //     p.setProgram("curl");
    //     p.setArguments({"https://www.baidu.com"});
    //     p.start();
    //     p.waitForStarted();
    //     p.waitForFinished(-1);
    //     qDebug() << p.readAllStandardOutput();
    //     qDebug() << "finish" << p.exitStatus() << p.state();
    // });
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::get();
    return pImpl->Install(packageIDList);
}

RetMessageList PackageManager::Uninstall(const QStringList &packageIDList)
{
    // 校验包名参数
    // 判断软件包是否安装
    // 卸载
    // 更新安装数据库
    // 更新本地软件包目录
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    if (packageIDList.size() == 0) {
        qInfo() << "packageIDList input err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("packageIDList input err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    return PackageManagerImpl::get()->Uninstall(packageIDList);
}

QString PackageManager::Update(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*
 * 查询软件包
 *
 * @param packageIDList: 软件包的appid
 *
 * @return PKGInfoList 查询结果列表
 */
PKGInfoList PackageManager::Query(const QStringList &packageIDList)
{
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return {};
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::get();
    return pImpl->Query(packageIDList);
}

/*!
 * 安装本地软件包
 * @param packagePathList
 */
QString PackageManager::Import(const QStringList &packagePathList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*!
 * 执行软件包
 * @param packageID 软件包的appid
 */
QString PackageManager::Start(const QString &packageID)
{
    Q_D(PackageManager);

    qDebug() << "start package" << packageID;
    return JobManager::instance()->CreateJob([=](Job *jr) {
        QString config = nullptr;
        if (fileExists("~/.linglong/" + packageID + ".yaml")) {
            config = "~/.linglong/" + packageID + ".yaml";
        } else if (fileExists("/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml")) {
            config = "/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml";
        } else {
            auto config_dir = listDirFolders("/deepin/linglong/layers/" + packageID);
            if (config_dir.isEmpty()) {
                qInfo() << "loader:: can not found config file!";
                return;
            }
            if (config_dir.size() >= 2) {
                std::sort(config_dir.begin(), config_dir.end(), [](const QString &s1, const QString &s2) {
                    auto v1 = s1.split("/").last();
                    auto v2 = s2.split("/").last();
                    return v1.toDouble() > v2.toDouble();
                });
            }
            config = config_dir.first() + "/" + packageID + ".yaml";
        }

        qDebug() << "load package" << packageID << " config " << config;
        // run org.deepin.calculator/ 配置文件不存在时会导致空指针
        if (!fileExists(config)) {
            qCritical() << config << " not exist";
            return;
        }
        auto app = App::load(config);
        if (nullptr == app) {
            qCritical() << "nullptr" << app;
            return;
        }
        d->apps[app->container->ID] = QPointer<App>(app);
        app->start();
    });
    //    sendErrorReply(QDBusError::NotSupported, message().member());
}

void PackageManager::Stop(const QString &containerID)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

ContainerList PackageManager::ListContainer()
{
    Q_D(PackageManager);
    ContainerList list;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->ID = app->container->ID;
        c->PID = app->container->PID;
        list.push_back(c);
    }
    return list;
}