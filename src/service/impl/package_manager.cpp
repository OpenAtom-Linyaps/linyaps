/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "package_manager.h"

#include <signal.h>
#include <sys/types.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSysInfo>

#include "module/runtime/app.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/fs.h"
#include "module/util/sysinfo.h"
#include "module/package/info.h"
#include "module/repo/repo.h"
#include "dbus_retcode.h"
#include "job_manager.h"
#include "module/repo/ostree.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;
using linglong::dbus::RetCode;

// using namespace linglong;

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
        , repo(linglong::repo::kRepoRoot)
    {
    }

    QMap<QString, QPointer<linglong::runtime::App>> apps;

    PackageManager *q_ptr = nullptr;

    linglong::repo::OSTree repo;
};

PackageManager::PackageManager()
    : dd_ptr(new PackageManagerPrivate(this))
{
    // 检查安装数据库信息
    checkInstalledAppDb();
    updateInstalledAppInfoDb();

    // 检查应用缓存信息
    checkAppCache();
}

PackageManager::~PackageManager() = default;

/*!
 * 下载软件包
 * @param paramOption
 */
linglong::service::Reply PackageManager::Download(const linglong::service::DownloadParamOption &downloadParamOption)
{
    linglong::service::Reply reply;
    QString appId = downloadParamOption.appId;
    if (appId.isNull() || appId.isEmpty()) {
        qCritical() << "package name err";
        reply.code = RetCode(RetCode::user_input_param_err);
        reply.message = "package name err";
        return reply;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    return pImpl->Download(downloadParamOption);
}

/*!
 * 在线安装软件包
 * @param installParamOption
 */
linglong::service::Reply PackageManager::Install(const linglong::service::InstallParamOption &installParamOption)
{
    linglong::service::Reply reply;
    QString appId = installParamOption.appId.trimmed();
    // 校验参数
    if (appId.isNull() || appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = RetCode(RetCode::user_input_param_err);
        return reply;
    }

    if ("flatpak" == installParamOption.repoPoint) {
        return PackageManagerFlatpakImpl::instance()->Install(installParamOption);
    }
    // JobManager::instance()->CreateJob([=]() {
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    reply = pImpl->Install(installParamOption);
    return reply;
    //});
    // return retMsg;
}

linglong::service::Reply PackageManager::Uninstall(const linglong::service::UninstallParamOption &paramOption)
{
    linglong::service::Reply reply;
    QString appId = paramOption.appId.trimmed();
    // 校验参数
    if (appId.isNull() || appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = RetCode(RetCode::user_input_param_err);
        return reply;
    }

    if ("flatpak" == paramOption.repoPoint) {
        return PackageManagerFlatpakImpl::instance()->Uninstall(paramOption);
    }

    return PackageManagerImpl::instance()->Uninstall(paramOption);
}

linglong::service::Reply PackageManager::Update(const linglong::service::ParamOption &paramOption)
{
    linglong::service::Reply reply;
    QString appId = paramOption.appId.trimmed();
    // 校验参数
    if (appId.isNull() || appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = RetCode(RetCode::user_input_param_err);
        return reply;
    }

    reply = PackageManagerImpl::instance()->Update(paramOption);
    return reply;
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*
 * 查询软件包
 *
 * @param paramOption 查询命令参数
 *
 * @return QueryReply dbus方法调用应答
 */
linglong::service::QueryReply PackageManager::Query(const linglong::service::QueryParamOption &paramOption)
{
    if ("flatpak" == paramOption.repoPoint) {
        return PackageManagerFlatpakImpl::instance()->Query(paramOption);
    }
    linglong::service::QueryReply reply;
    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        reply.code = RetCode(RetCode::user_input_param_err);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return reply;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    return pImpl->Query(paramOption);
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

/*
 * 执行软件包
 *
 * @param paramOption 启动命令参数
 *
 * @return linglong::service::Reply: 运行结果信息
 */
linglong::service::Reply PackageManager::Start(const linglong::service::RunParamOption &paramOption)
{
    Q_D(PackageManager);

    linglong::service::QueryReply reply;
    reply.code = 0;
    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        reply.code = RetCode(RetCode::user_input_param_err);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return reply;
    }

    QString arch = paramOption.arch.trimmed();
    if (arch.isNull() || arch.isEmpty()) {
        arch = hostArch();
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

    // 判断是否已安装
    if (!getAppInstalledStatus(appId, version, "", "")) {
        reply.message = appId + " not installed";
        qCritical() << reply.message;
        reply.code = RetCode(RetCode::pkg_not_installed);
        return reply;
    }
    QString repoPoint = paramOption.repoPoint;
    JobManager::instance()->CreateJob([=]() {
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
    return reply;
}

/*
 * 停止应用
 *
 * @param containerId: 应用启动对应的容器Id
 *
 * @return linglong::service::Reply 执行结果信息
 */
linglong::service::Reply PackageManager::Stop(const QString &containerId)
{
    Q_D(PackageManager);

    linglong::service::Reply reply;
    auto it = d->apps.find(containerId);
    if (it == d->apps.end()) {
        reply.code = RetCode(RetCode::user_input_param_err);
        reply.message = "containerId:" + containerId + " not exist";
        qCritical() << reply.message;
        return reply;
    }
    auto app = it->data();
    pid_t pid = app->container()->pid;
    int ret = kill(pid, SIGKILL);
    if (ret != 0) {
        reply.message = "kill container failed, containerId:" + containerId;
        reply.code = RetCode(RetCode::ErrorPkgKillFailed);
    } else {
        reply.code = RetCode(RetCode::ErrorPkgKillSuccess);
        reply.message = "kill app:" + app->container()->packageName + " success";
    }
    qInfo() << "kill containerId:" << containerId << ",ret:" << ret;
    return reply;
}

ContainerList PackageManager::ListContainer()
{
    Q_D(PackageManager);
    ContainerList list;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->id = app->container()->id;
        c->pid = app->container()->pid;
        c->packageName = app->container()->packageName;
        c->workingDirectory = app->container()->workingDirectory;
        list.push_back(c);
    }
    return list;
}
QString PackageManager::Status()
{
    return {"active"};
}
