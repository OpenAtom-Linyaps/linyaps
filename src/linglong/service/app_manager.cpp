/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app_manager.h"

#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/app.h"
#include "linglong/util/app_status.h"
#include "linglong/util/connection.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/utils/error/error.h"

#include <csignal>

#include <sys/types.h>

namespace linglong::service {

AppManager::AppManager(repo::Repo &repo)
    : runPool(new QThreadPool)
    , repo(repo)
{
    runPool->setMaxThreadCount(RUN_POOL_MAX_THREAD);
}

linglong::utils::error::Result<void> AppManager::Run(const RunParamOption &paramOption)
{
    qDebug() << "run" << paramOption.appId;

    QString appID = paramOption.appId;
    QString arch = paramOption.arch;
    QString version = paramOption.version;
    QString channel = paramOption.channel;
    QString module = paramOption.appModule;
    QStringList exec = paramOption.exec;
    QStringList env = paramOption.appEnv;
    // 默认使用主机架构
    if (arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }
    // 默认运行应用的runtime，而不是devel
    if (module.isEmpty()) {
        module = "runtime";
    }
    // 玲珑早期版本的默认channel是 linglong，后来改成 main，需要进行兼容
    // TODO(wurongjie) 在版本迭代后，可以删除对旧 channel 的支持
    if (channel.isEmpty()) {
        for (auto defaultChannel : QStringList{ "main", "linglong" }) {
            auto ret = repo.localLatestVersion(appID, arch, defaultChannel, module);
            if (ret.has_value()) {
                channel = defaultChannel;
                break;
            } else if (ret.error().code() != 404) {
                return LINGLONG_EWRAP("select default channel err", ret.error());
            }
        }
        if (channel.isEmpty()) {
            return LINGLONG_ERR(
              -1,
              QString("Application %1/%2/$latest/%3/%4 cannot be found from the database, "
                      "maybe you should install it first or pass more parameter")
                .arg("main")
                .arg(appID)
                .arg(arch)
                .arg(module));
        }
    }
    // 默认选择最新的版本
    if (version.isEmpty()) {
        auto ret = repo.localLatestVersion(appID, arch, channel, module);
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("get local latest version", ret.error());
        }
        version = *ret;
    }
    // 加载应用
    linglong::package::Ref ref("", channel, appID, version, arch, module);
    qDebug() << "app load ref:" << ref.toSpecString() << "exec" << exec;
    auto app = linglong::runtime::App::load(&repo, ref, exec);
    if (app == nullptr) {
        return LINGLONG_ERR(-1, "load app failed");
    }
    // 设置环境变量
    app->saveUserEnvList(env);
    // 设置应用参数
    ParamStringMap paramMap;
    paramMap.insert(linglong::util::kKeyNoProxy, "");
    app->setAppParamMap(paramMap);
    // 启动应用
    auto err = app->start();
    if (err) {
        return LINGLONG_ERR(err.code(), err.message());
    }
    return LINGLONG_OK;
}

auto AppManager::Start(const RunParamOption &paramOption) -> Reply
{
    qDebug() << "start" << paramOption.appId;

    QueryReply reply;
    reply.code = 0;
    reply.message = "Start " + paramOption.appId + " success!";
    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return std::move(reply);
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
        userEnvList = paramOption.appEnv;
    }

    // 获取exec参数
    QStringList desktopExec;
    if (!paramOption.exec.isEmpty()) {
        desktopExec = paramOption.exec;
    }

    QString channel = paramOption.channel.trimmed();
    QString appModule = paramOption.appModule.trimmed();
    if (appModule.isEmpty()) {
        appModule = "runtime";
    }
    // 玲珑早期版本的默认channel是 linglong，后来改成 main，需要进行兼容
    // TODO(wurongjie) 在版本迭代后，可以删除对旧 channel 的支持
    if (channel.isEmpty()) {
        for (auto defaultChannel : QStringList{ "main", "linglong" }) {
            if (util::getAppInstalledStatus(appId, version, arch, defaultChannel, appModule, "")) {
                channel = defaultChannel;
                break;
            }
        }
    }
    // 判断是否已安装
    if (!linglong::util::getAppInstalledStatus(appId, version, arch, channel, appModule, "")) {
        reply.message = appId + ", version:" + version + ", arch:" + arch + ", channel:" + channel
          + ", module:" + appModule + " not installed";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kPkgNotInstalled);
        return std::move(reply);
    }

    // 直接运行debug版本时，校验release包是否安装
    if ("devel" == appModule) {
        QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
        linglong::util::getAllVerAppInfo(appId, version, arch, "", pkgList);
        if (pkgList.size() < 2) {
            reply.message = appId + ", version:" + version + ", arch:" + arch + ", channel:"
              + channel + ", module:" + appModule + ", no corresponding release package found";
            qCritical() << reply.message;
            reply.code = STATUS_CODE(kPkgNotInstalled);
            return std::move(reply);
        }
    }

    // 链接${LINGLONG_ROOT}/entries/share到~/.config/systemd/user下
    // FIXME:后续上了提权模块，放入安装处理。
    const QString appUserServicePath =
      linglong::util::getLinglongRootPath() + "/entries/share/systemd/user";
    const QString userSystemdServicePath =
      linglong::util::ensureUserDir({ ".config/systemd/user" });
    if (linglong::util::dirExists(appUserServicePath)) {
        linglong::util::linkDirFiles(appUserServicePath, userSystemdServicePath);
    }

    // FIXME: report error here.
    QFuture<void> future = QtConcurrent::run(runPool.data(), [=]() {
        // 判断是否存在
        linglong::package::Ref ref("", channel, appId, version, arch, appModule);

        // 判断是否是正在运行应用
        auto latestAppRef = repo.latestOfRef(appId, version);
        for (const auto &app : apps) {
            if (latestAppRef.toString() == app->container->packageName) {
                app->exec(desktopExec, {}, "");
                return;
            }
        }

        auto app = linglong::runtime::App::load(&repo, ref, desktopExec);
        if (nullptr == app) {
            // FIXME: set job status to failed
            qCritical() << "load app failed " << app;
            return;
        }
        app->saveUserEnvList(userEnvList);
        app->setAppParamMap(paramMap);
        auto it = apps.insert(app->container->id, app);
        auto err = app->start();
        if (err) {
            qCritical() << "start app failed" << err;
        }
        apps.erase(it);
    });
    // future.waitForFinished();
    return std::move(reply);
}

Reply AppManager::Exec(const ExecParamOption &paramOption)
{
    Reply reply;
    reply.code = STATUS_CODE(kFail);
    reply.message = "No such container " + paramOption.containerID;
    auto const &containerID = paramOption.containerID;
    for (auto it : apps) {
        if (it->container->id == containerID
            // FIXME: use new reference
            || package::Ref(it->container->packageName).appId == containerID) {
            it->exec(paramOption.cmd, paramOption.env, paramOption.cwd);
            reply.code = STATUS_CODE(kSuccess);
            reply.message = "Exec successed";
            return reply;
        }
    }
    return reply;
}

Reply AppManager::Stop(const QString &containerId)
{
    Reply reply;
    auto it = apps.find(containerId);
    if (it == apps.end()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "containerId:" + containerId + " not exist";
        qCritical() << reply.message;
        return reply;
    }
    auto app = it->data();
    pid_t pid = app->container->pid;
    int ret = kill(pid, SIGKILL);
    if (ret != 0) {
        reply.message = "kill container failed, containerId:" + containerId;
        reply.code = STATUS_CODE(kErrorPkgKillFailed);
    } else {
        reply.code = STATUS_CODE(kErrorPkgKillSuccess);
        reply.message = "kill app:" + app->container->packageName + " success";
    }
    qInfo() << "kill containerId:" << containerId << ",ret:" << ret;
    return reply;
}

QueryReply AppManager::ListContainer()
{
    QJsonArray jsonArray;

    for (const auto &app : apps) {
        auto container = QSharedPointer<Container>(new Container);
        container->id = app->container->id;
        container->pid = app->container->pid;
        container->packageName = app->container->packageName;
        container->workingDirectory = app->container->workingDirectory;
        jsonArray.push_back(QJsonObject::fromVariantMap(QVariant::fromValue(container).toMap()));
    }

    QueryReply reply;
    reply.code = STATUS_CODE(kSuccess);
    reply.message = "Success";
    reply.result = QLatin1String(QJsonDocument(jsonArray).toJson(QJsonDocument::Compact));
    return reply;
}

} // namespace linglong::service
