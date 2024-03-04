/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app_manager.h"

#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/app.h"
#include "linglong/util/app_status.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/utils/error/error.h"
#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <QString>

namespace linglong::service {

AppManager::AppManager(repo::Repo &repo, ocppi::cli::CLI &ociCli)
    : runPool(new QThreadPool)
    , repo(repo)
    , ociCli(ociCli)
{
    runPool->setMaxThreadCount(RUN_POOL_MAX_THREAD);
}

linglong::utils::error::Result<void> AppManager::Run(const RunParamOption &paramOption)
{
    LINGLONG_TRACE(QString("run %1").arg(paramOption.appId));

    qDebug() << "run" << paramOption.appId;

    QString appID = paramOption.appId;
    QString arch = paramOption.arch;
    QString version = paramOption.version;
    QString channel = paramOption.channel;
    QString module = paramOption.appModule;
    QStringList exec = paramOption.exec;
    QStringList env = paramOption.appEnv;
    // 默认使用当前架构
    if (arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }
    // 默认运行应用的runtime而不是devel
    if (module.isEmpty()) {
        module = "runtime";
    }
    // 玲珑早期版本的默认channel是 linglong，后来改成 main，需要进行兼容
    // TODO(wurongjie) 在版本迭代后，可以删除对旧 channel 的支持
    if (channel.isEmpty()) {
        auto refs = repo.listLocalRefs();
        if (!refs) {
            return LINGLONG_ERR(refs);
        }
        for (auto ref : *refs) {
            if (ref.appId == appID && ref.arch == arch && ref.module == module) {
                // 优先使用 main channel
                if (ref.channel == "main") {
                    channel = "main";
                    break;
                }
                // 兼容旧的 linglong channel
                if (ref.channel == "linglong") {
                    channel = "linglong";
                }
            }
        }
        if (channel.isEmpty()) {
            return LINGLONG_ERR(
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
        auto ret = repo.latestOfRef(appID, "");
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        version = ret->version;
    }
    // 加载应用
    linglong::package::Ref ref("", channel, appID, version, arch, module);
    auto app = linglong::runtime::App::load(&repo, ref, exec, &ociCli);
    if (app == nullptr) {
        return LINGLONG_ERR("load app failed");
    }
    // 设置环境变量
    app->saveUserEnvList(env);
    // 设置应用参数
    ParamStringMap paramMap;
    paramMap.insert(linglong::util::kKeyNoProxy, "");
    app->setAppParamMap(paramMap);
    // 启动应用
    auto ret = app->start();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    return LINGLONG_OK;
}

Reply AppManager::Exec(const ExecParamOption &paramOption)
{
    LINGLONG_TRACE("exec");

    Reply reply;
    auto const &containerID = paramOption.containerID;

    std::string executable = paramOption.cmd[0].toStdString();
    std::vector<std::string> command;
    for (const auto &cmd : paramOption.cmd.mid(1)) {
        command.push_back(cmd.toStdString());
    }
    ocppi::runtime::ExecOption opt;
    opt.tty = true;
    opt.uid = getuid();
    opt.gid = getgid();
    auto ret = ociCli.exec(containerID.toStdString(), executable, command, { opt });
    if (!ret) {
        auto err = LINGLONG_ERR(ret.error());
        reply.code = STATUS_CODE(kFail);
        reply.message = "exec container failed: " + err.value().message();
        return reply;
    }
    reply.code = STATUS_CODE(kSuccess);
    return reply;
}

Reply AppManager::Stop(const QString &containerId)
{
    LINGLONG_TRACE("stop");

    Reply reply;
    auto ret = ociCli.kill(containerId.toStdString(), "SIGTERM");
    if (!ret.has_value()) {
        reply.message = "kill container failed, containerId:" + containerId;
        reply.code = STATUS_CODE(kErrorPkgKillFailed);
    } else {
        reply.code = STATUS_CODE(kErrorPkgKillSuccess);
        reply.message = "kill container success, containerId:" + containerId;
    }
    auto err = LINGLONG_ERR(ret.error());
    qInfo() << "kill containerId:" << containerId << ", ret:" << err.value().message();
    return reply;
}

QueryReply AppManager::ListContainer()
{
    LINGLONG_TRACE("list");

    QueryReply reply;
    auto ret = ociCli.list();
    if (!ret) {
        auto err = LINGLONG_ERR(ret.error());
        reply.code = err.value().code();
        reply.message = err.value().message();
        return reply;
    }
    QJsonArray jsonArray;
    for (const auto &item : *ret) {
        auto container = QSharedPointer<Container>(new Container);
        container->id = QString::fromStdString(item.id);
        container->pid = item.pid;
        container->workingDirectory = QString::fromStdString(item.bundle);
        container->packageName = QDir(QString::fromStdString(item.bundle)).dirName();
        jsonArray.push_back(QJsonObject::fromVariantMap(QVariant::fromValue(container).toMap()));
    }

    reply.code = STATUS_CODE(kSuccess);
    reply.message = "Success";
    reply.result = QLatin1String(QJsonDocument(jsonArray).toJson(QJsonDocument::Compact));
    return reply;
}

} // namespace linglong::service
