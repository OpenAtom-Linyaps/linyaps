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

using linglong::service::util::AppInstance;
using linglong::service::util::AppInfo;

using namespace linglong;

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
        , repo(repo::kRepoRoot)
    {
    }

    QMap<QString, QPointer<runtime::App>> apps;

    PackageManager *q_ptr = nullptr;

    repo::OSTree repo;
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
 * @param packageIdList
 */
RetMessageList PackageManager::Download(const QStringList &packageIdList, const QString savePath)
{
    // Q_D(PackageManager);

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
    QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    return pImpl->Download(packageIdList, savePath);
}

/*!
 * 在线安装软件包
 * @param packageIdList
 */
RetMessageList PackageManager::Install(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_REPO_POINT)) {
        return PackageManagerFlatpakImpl::instance()->Install(packageIdList);
    }
    // Q_D(PackageManager);

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
    QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    return pImpl->Install(packageIdList, paramMap);
}

RetMessageList PackageManager::Uninstall(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_REPO_POINT)) {
        return PackageManagerFlatpakImpl::instance()->Uninstall(packageIdList);
    }
    // 校验包名参数
    // 判断软件包是否安装
    // 卸载
    // 更新安装数据库
    // 更新本地软件包目录
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    if (packageIdList.size() == 0) {
        qInfo() << "packageIdList input err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("packageIdList input err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    return PackageManagerImpl::instance()->Uninstall(packageIdList, paramMap);
}

void PackageManager::Update(const linglong::service::PackageManagerOptionList &opts)
{
    auto option = linglong::service::unwrapOption(opts);
    if (option.isNull()) {
        return sendErrorReply(QDBusError::InvalidArgs,
                              NewError(static_cast<int>(RetCode::user_input_param_err), "package list empty").toJson());
    }

    if (option->ref.isEmpty()) {
        sendErrorReply(QDBusError::InvalidArgs,
                       NewError(static_cast<int>(RetCode::user_input_param_err), "package name err").toJson());
        return;
    }

    auto ref = package::Ref(option->ref);

    auto err = PackageManagerImpl::instance()->Update(ref);
    if (!err.success()) {
        sendErrorReply(QDBusError::Failed, err.toJson());
        qWarning() << err;
    }
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*
 * 查询软件包
 *
 * @param packageIdList: 软件包的appId
 *
 * @return AppMetaInfoList 查询结果列表
 */
AppMetaInfoList PackageManager::Query(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_REPO_POINT)) {
        return PackageManagerFlatpakImpl::instance()->Query(packageIdList);
    }
    QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return {};
    }
    PackageManagerProxyBase *pImpl = PackageManagerImpl::instance();
    return pImpl->Query(packageIdList, paramMap);
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
 * @param opts: 软件包运行参数
 *
 * @return RetMessageList: 运行结果信息
 */
RetMessageList PackageManager::Start(const linglong::service::PackageManagerOptionList &opts)
{
    Q_D(PackageManager);

    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);

    //获取user env list
    auto option = linglong::service::unwrapOption(opts);
    if (option.isNull()) {
        sendErrorReply(QDBusError::InvalidArgs,
                       NewError(static_cast<int>(RetCode::user_input_param_err), "package list empty").toJson());
    }
    if (option->runParamOption->envList.isEmpty()) {
        sendErrorReply(QDBusError::InvalidArgs,
                       NewError(static_cast<int>(RetCode::user_env_list_err), "get usr env list err").toJson());
    }

    auto appRef = package::Ref(option->ref);

    // 获取appId
    const QString packageId = appRef.appId;

    // 获取版本信息
    QString version = appRef.version;
    if ("latest" == version) {
        version = "";
    }

    // 获取envList
    auto envList = option->runParamOption->envList;

    // 获取exec参数
    auto desktopExec = option->runParamOption->exec;

    // 获取repoPoint
    auto repoPoint = option->runParamOption->repoPoint;

    // 判断是否已安装
    QString err = "";
    if (!getAppInstalledStatus(packageId, version, "", "")) {
        err = packageId + " not installed";
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_not_installed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    JobManager::instance()->CreateJob([=]() {
        // 判断是否存在
        package::Ref ref("", packageId, version, hostArch());

        bool isFlatpakApp = !repoPoint.isNull() && !repoPoint.isEmpty();

        auto app = runtime::App::load(&d->repo, ref, desktopExec, isFlatpakApp);
        if (nullptr == app) {
            // FIXME: set job status to failed
            qCritical() << "nullptr" << app;
            return;
        }
        app->saveUserEnvList(envList);
        d->apps[app->container()->id] = QPointer<runtime::App>(app);
        app->start();
    });
    return retMsg;
}

/*
 * 停止应用
 *
 * @param containerId: 应用启动对应的容器Id
 *
 * @return RetMessageList 执行结果信息
 */
RetMessageList PackageManager::Stop(const QString &containerId)
{
    Q_D(PackageManager);

    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    QString err = "";
    if (!d->apps.contains(containerId)) {
        err = "containerId:" + containerId + " not exist";
        qCritical() << err;
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    pid_t pid = d->apps[containerId]->container()->pid;
    int ret = kill(pid, SIGKILL);
    if (ret == 0) {
        d->apps.remove(containerId);
    } else {
        err = "kill container failed, containerId:" + containerId;
        info->setcode(RetCode(RetCode::ErrorPkgKillFailed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
    }
    qInfo() << "kill containerId:" << containerId << ",ret:" << ret;
    return retMsg;
}

ContainerList PackageManager::ListContainer()
{
    Q_D(PackageManager);
    ContainerList list;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->id = app->container()->id;
        c->pid = app->container()->pid;
        list.push_back(c);
    }
    return list;
}
QString PackageManager::Status()
{
    return {"active"};
}
