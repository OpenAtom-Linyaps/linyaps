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

#include <sys/types.h>
#include <signal.h>

#include "module/runtime/app.h"
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

bool PackageManagerPrivate::isAppRunning(const QString &appId, const QString &version, const QString &arch)
{
    linglong::package::AppMetaInfoList pkgList;
    if (!appId.isEmpty()) {
        linglong::util::getInstalledAppInfo(appId, version, arch, "", pkgList);
        if (pkgList.size() > 0) {
            auto it = pkgList.at(0);
            // new ref format org.deepin.calculator/1.2.2/x86_64
            QString matchRef = QString("%1/%2/%3").arg(it->appId).arg(it->version).arg(arch);
            // 判断应用是否正在运行
            for (const auto &app : apps) {
                if (matchRef == app->container()->packageName) {
                    return true;
                }
            }
        }
    }
    return false;
}

/*!
 * 下载软件包
 * @param paramOption
 */
Reply PackageManager::Download(const DownloadParamOption &downloadParamOption)
{
    linglong::service::Reply reply;
    return reply;
}

/**
 * @brief 查询软件包下载安装状态
 *
 * @param paramOption 查询参数
 * @param type 查询类型 0:查询应用安装进度 1:查询应用更新进度
 *
 * @return Reply dbus方法调用应答 \n
 *          code:状态码 \n
 *          message:信息
 */
Reply PackageManager::GetDownloadStatus(const ParamOption &paramOption, int type)
{
    linglong::service::Reply reply;
    return reply;
}

/*!
 * 在线安装软件包
 * @param installParamOption
 */
Reply PackageManager::Install(const InstallParamOption &installParamOption)
{
    // QDBusInterface interface("com.deepin.linglong.SystemPackageManager", "/com/deepin/linglong/SystemPackageManager",
    //                          "com.deepin.linglong.SystemPackageManager", QDBusConnection::systemBus());
    // 设置 24 h超时
    // interface.setTimeout(1000 * 60 * 60 * 24);
    // QDBusPendingReply<Reply> dbusReply = interface.call("Install", QVariant::fromValue(installParamOption));
    // dbusReply.waitForFinished();
    linglong::service::Reply reply;
    // reply.code = STATUS_CODE(kPkgInstalling);
    // if (dbusReply.isValid()) {
    //     reply = dbusReply.value();
    // }
    qInfo() << "PackageManager::Install called";
    return reply;
}

Reply PackageManager::Uninstall(const UninstallParamOption &paramOption)
{
    linglong::service::Reply reply;
    qInfo() << "PackageManager::Uninstall called";
    return reply;
}

Reply PackageManager::Update(const ParamOption &paramOption)
{
    linglong::service::Reply reply;
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
    linglong::service::QueryReply reply;
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
    // FIXME:后续上了提权模块，放入安装处理。
    const QString appUserServicePath = linglong::util::getLinglongRootPath() + "/entries/share/systemd/user";
    const QString userSystemdServicePath = linglong::util::ensureUserDir({".config/systemd/user"});
    if (linglong::util::dirExists(appUserServicePath)) {
        linglong::util::linkDirFiles(appUserServicePath, userSystemdServicePath);
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
