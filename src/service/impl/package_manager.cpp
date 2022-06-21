/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// todo: 该头文件必须放在QDBus前，否则会报错
#include "module/repo/ostree_repohelper.h"

#include "package_manager.h"

#include <QDebug>
#include <QDBusInterface>
#include <QDBusReply>

#include "module/runtime/app.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/file.h"
#include "module/util/sysinfo.h"
#include "module/package/info.h"
#include "module/repo/repo.h"
#include "job_manager.h"
#include "module/repo/ostree.h"
#include "package_manager_p.h"

namespace linglong {
namespace service {

PackageManagerPrivate::PackageManagerPrivate(PackageManager *parent)
    : repo(util::getLinglongRootPath())
    , q_ptr(parent)
{
}

PackageManager::PackageManager()
    : runPool(new QThreadPool)
    , dd_ptr(new PackageManagerPrivate(this))
{
    runPool->setMaxThreadCount(RUN_POOL_MAX_THREAD);
}

PackageManager::~PackageManager()
{
}

/*!
 * 下载软件包
 * @param paramOption
 */
Reply PackageManager::Download(const DownloadParamOption &downloadParamOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    // 设置 24 h超时
    interface.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<Reply> dbusReply = interface.call("Download", QVariant::fromValue(downloadParamOption));
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply.code = STATUS_CODE(kSuccess);
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

/**
 * @brief 查询软件包下载安装状态
 *
 * @param paramOption 查询参数
 *
 * @return Reply dbus方法调用应答 \n
 *          code:状态码 \n
 *          message:信息
 */
Reply PackageManager::GetDownloadStatus(const ParamOption &paramOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    QDBusPendingReply<Reply> dbusReply = interface.call("GetDownloadStatus", QVariant::fromValue(paramOption));
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply.code = STATUS_CODE(kSuccess);
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

/*!
 * 在线安装软件包
 * @param installParamOption
 */
Reply PackageManager::Install(const InstallParamOption &installParamOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    // 设置 24 h超时
    interface.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<Reply> dbusReply = interface.call("Install", QVariant::fromValue(installParamOption));
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply.code = STATUS_CODE(kPkgInstalling);
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

Reply PackageManager::Uninstall(const UninstallParamOption &paramOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    // 设置 30分钟超时
    interface.setTimeout(1000 * 60 * 30);
    QDBusPendingReply<Reply> dbusReply = interface.call("Uninstall", QVariant::fromValue(paramOption));
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply.code = STATUS_CODE(kPkgUninstalling);
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

Reply PackageManager::Update(const ParamOption &paramOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    QDBusPendingReply<Reply> dbusReply = interface.call("Update", QVariant::fromValue(paramOption));
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply.code = STATUS_CODE(kPkgUpdating);
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

/*
 * 查询软件包
 *
 * @param paramOption 查询命令参数
 *
 * @return QueryReply dbus方法调用应答
 */
QueryReply PackageManager::Query(const QueryParamOption &paramOption)
{
    QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager", "com.deepin.linglong.SystemPackageManager",
                             QDBusConnection::systemBus());
    QDBusPendingReply<QueryReply> dbusReply = interface.call("Query", QVariant::fromValue(paramOption));
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply;
    if (dbusReply.isValid()) {
        reply = dbusReply.value();
    }
    return reply;
}

/*
 * 执行软件包
 *
 * @param paramOption 启动命令参数
 *
 * @return Reply: 运行结果信息
 */
Reply PackageManager::Start(const RunParamOption &paramOption)
{
    Q_D(PackageManager);

    QueryReply reply;
    reply.code = 0;
    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return reply;
    }

    QString arch = paramOption.arch.trimmed();
    if (arch.isNull() || arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }

    ParamStringMap paramMap;
    // 获取版本信息
    QString version = "";
    if (!paramOption.version.isEmpty()) {
        version = paramOption.version.trimmed();
    }

    if (paramOption.noDbusProxy) {
        paramMap.insert(linglong::util::kKeyNoProxy, "");
    }

    if (!paramOption.noDbusProxy) {
        // FIX to do only deal with session bus
        paramMap.insert(linglong::util::kKeyBusType, paramOption.busType);
        auto nameFilter = paramOption.filterName.trimmed();
        if (!nameFilter.isEmpty()) {
            paramMap.insert(linglong::util::kKeyFilterName, nameFilter);
        }
        auto pathFilter = paramOption.filterPath.trimmed();
        if (!pathFilter.isEmpty()) {
            paramMap.insert(linglong::util::kKeyFilterPath, pathFilter);
        }
        auto interfaceFilter = paramOption.filterInterface.trimmed();
        if (!interfaceFilter.isEmpty()) {
            paramMap.insert(linglong::util::kKeyFilterIface, interfaceFilter);
        }
    }
    // 获取user env list
    QStringList userEnvList;
    if (!paramOption.appEnv.isEmpty()) {
        userEnvList = paramOption.appEnv.split(",");
    }

    // 获取exec参数
    QString desktopExec;
    desktopExec.clear();
    if (!paramOption.exec.isEmpty()) {
        desktopExec = paramOption.exec;
    }

    QString repoPoint = paramOption.repoPoint;
    if ("flatpak" != repoPoint) {
        // 判断是否已安装
        if (!linglong::util::getAppInstalledStatus(appId, version, "", "")) {
            reply.message = appId + " not installed";
            qCritical() << reply.message;
            reply.code = STATUS_CODE(kPkgNotInstalled);
            return reply;
        }
    }

    //链接${LINGLONG_ROOT}/entries/share到~/.config/systemd/user下
    //FIXME:后续上了提权模块，放入安装处理。
    const QString appUserServicePath = linglong::util::getLinglongRootPath() + "/entries/share/systemd/user";
    const QString userSystemdServicePath = linglong::util::ensureUserDir({".config/systemd/user"});
    if(linglong::util::dirExists(appUserServicePath)){
        linglong::util::linkDirFiles(appUserServicePath,userSystemdServicePath);
    }

    QFuture<void> future = QtConcurrent::run(runPool.data(), [=]() {
        // 判断是否存在
        linglong::package::Ref ref("", appId, version, arch);

        bool isFlatpakApp = "flatpak" == repoPoint;

        auto app = linglong::runtime::App::load(&d->repo, ref, desktopExec, isFlatpakApp);
        if (nullptr == app) {
            // FIXME: set job status to failed
            qCritical() << "nullptr" << app;
            return;
        }
        app->saveUserEnvList(userEnvList);
        app->setAppParamMap(paramMap);
        auto it = d->apps.insert(app->container()->id, QPointer<linglong::runtime::App>(app));
        app->start();
        d->apps.erase(it);
        app->deleteLater();
    });
    // future.waitForFinished();
    return reply;
}

Reply PackageManager::Exec(const ExecParamOption &paramOption)
{
    Q_D(PackageManager);
    Reply reply;
    reply.code = STATUS_CODE(kFail);
    auto const &containerID = paramOption.containerID;
    for (auto it : d->apps) {
        if (it->container()->id == containerID) {
            it->exec(paramOption.cmd, paramOption.env, paramOption.cwd);
            reply.code = STATUS_CODE(kSuccess);
            return reply;
        }
    }
    return reply;
}

/*
 * 停止应用
 *
 * @param containerId: 应用启动对应的容器Id
 *
 * @return Reply 执行结果信息
 */
Reply PackageManager::Stop(const QString &containerId)
{
    Q_D(PackageManager);
    Reply reply;
    auto it = d->apps.find(containerId);
    if (it == d->apps.end()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "containerId:" + containerId + " not exist";
        qCritical() << reply.message;
        return reply;
    }
    auto app = it->data();
    pid_t pid = app->container()->pid;
    int ret = kill(pid, SIGKILL);
    if (ret != 0) {
        reply.message = "kill container failed, containerId:" + containerId;
        reply.code = STATUS_CODE(kErrorPkgKillFailed);
    } else {
        reply.code = STATUS_CODE(kErrorPkgKillSuccess);
        reply.message = "kill app:" + app->container()->packageName + " success";
    }
    qInfo() << "kill containerId:" << containerId << ",ret:" << ret;
    return reply;
}

QueryReply PackageManager::ListContainer()
{
    Q_D(PackageManager);
    QJsonArray jsonArray;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->id = app->container()->id;
        c->pid = app->container()->pid;
        c->packageName = app->container()->packageName;
        c->workingDirectory = app->container()->workingDirectory;
        jsonArray.push_back(c->toJson(c.data()));
    }

    QueryReply r;
    r.code = STATUS_CODE(kSuccess);
    r.message = "";
    r.result = QLatin1String(QJsonDocument(jsonArray).toJson(QJsonDocument::Compact));
    return r;
}

QString PackageManager::Status()
{
    return {"active"};
}
} // namespace service
} // namespace linglong
