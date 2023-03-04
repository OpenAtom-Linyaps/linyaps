/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app_manager.h"

#include "app_manager_p.h"
#include "module/runtime/app.h"
#include "module/util/app_status.h"
#include "module/util/file.h"
#include "module/util/status_code.h"
#include "module/util/sysinfo.h"

#include <signal.h>
#include <sys/types.h>

namespace linglong {
namespace service {

AppManagerPrivate::AppManagerPrivate(AppManager *parent)
    : repo(util::getLinglongRootPath())
    , q_ptr(parent)
{
}

AppManager::AppManager()
    : runPool(new QThreadPool)
    , dd_ptr(new AppManagerPrivate(this))
{
    runPool->setMaxThreadCount(RUN_POOL_MAX_THREAD);
}

AppManager::~AppManager() { }

/*
 * 执行软件包
 *
 * @param paramOption 启动命令参数
 *
 * @return Reply: 运行结果信息
 */
Reply AppManager::Start(const RunParamOption &paramOption)
{
    Q_D(AppManager);

    QueryReply reply;
    reply.code = 0;
    reply.message = "Start " + paramOption.appId + " success!";
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

    QString channel = paramOption.channel.trimmed();
    QString appModule = paramOption.appModule.trimmed();
    if (channel.isEmpty()) {
        channel = "linglong";
    }
    if (appModule.isEmpty()) {
        appModule = "runtime";
    }

    // 判断是否已安装
    if (!linglong::util::getAppInstalledStatus(appId, version, arch, channel, appModule, "")) {
        reply.message = appId + ", version:" + version + ", arch:" + arch + ", channel:" + channel
                + ", module:" + appModule + " not installed";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kPkgNotInstalled);
        return reply;
    }

    // 直接运行debug版本时，校验release包是否安装
    if ("devel" == appModule) {
        linglong::package::AppMetaInfoList pkgList;
        linglong::util::getAllVerAppInfo(appId, version, arch, "", pkgList);
        if (pkgList.size() < 2) {
            reply.message = appId + ", version:" + version + ", arch:" + arch
                    + ", channel:" + channel + ", module:" + appModule
                    + ", no corresponding release package found";
            qCritical() << reply.message;
            reply.code = STATUS_CODE(kPkgNotInstalled);
            return reply;
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

    QFuture<void> future = QtConcurrent::run(runPool.data(), [=]() {
        // 判断是否存在
        linglong::package::Ref ref("", channel, appId, version, arch, appModule);

        // 判断是否是正在运行应用
        auto latestAppRef = d->repo.latestOfRef(appId, version);
        for (const auto &app : d->apps) {
            if (latestAppRef.toString() == app->container()->packageName) {
                app->exec(desktopExec, "", "");
                return;
            }
        }

        auto app = linglong::runtime::App::load(&d->repo, ref, desktopExec);
        if (nullptr == app) {
            // FIXME: set job status to failed
            qCritical() << "load app failed " << app;
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

Reply AppManager::Exec(const ExecParamOption &paramOption)
{
    Q_D(AppManager);
    Reply reply;
    reply.code = STATUS_CODE(kFail);
    reply.message = "No such container " + paramOption.containerID;
    auto const &containerID = paramOption.containerID;
    for (auto it : d->apps) {
        if (it->container()->id == containerID) {
            it->exec(paramOption.cmd, paramOption.env, paramOption.cwd);
            reply.code = STATUS_CODE(kSuccess);
            reply.message = "Exec successed";
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
Reply AppManager::Stop(const QString &containerId)
{
    Q_D(AppManager);
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

QueryReply AppManager::ListContainer()
{
    Q_D(AppManager);
    QJsonArray jsonArray;

    for (const auto &app : d->apps) {
        auto container = QPointer<Container>(new Container);
        container->id = app->container()->id;
        container->pid = app->container()->pid;
        container->packageName = app->container()->packageName;
        container->workingDirectory = app->container()->workingDirectory;
        jsonArray.push_back(container->toJson(container.data()));
    }

    QueryReply reply;
    reply.code = STATUS_CODE(kSuccess);
    reply.message = "Success";
    reply.result = QLatin1String(QJsonDocument(jsonArray).toJson(QJsonDocument::Compact));
    return reply;
}

/**
 * @brief 执行终端命令
 *
 * @param exe 命令
 * @param args 参数
 *
 * @return Reply 执行结果信息
 */
Reply AppManager::RunCommand(const QString &exe, const QStringList args)
{
    Reply reply;
    QProcess process;
    process.setProgram(exe);

    process.setArguments(args);

    QProcess::connect(&process, &QProcess::readyReadStandardOutput, [&]() {
        std::cout << process.readAllStandardOutput().toStdString().c_str();
    });

    QProcess::connect(&process, &QProcess::readyReadStandardError, [&]() {
        std::cout << process.readAllStandardError().toStdString().c_str();
    });

    process.start();
    process.waitForStarted(15 * 60 * 1000);
    process.waitForFinished(15 * 60 * 1000);

    reply.code = process.exitCode();
    reply.message = process.errorString();

    return reply;
}

QString AppManager::Status()
{
    return "active";
}
} // namespace service
} // namespace linglong
