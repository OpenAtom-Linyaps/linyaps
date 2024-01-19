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
        if (!refs.has_value()) {
            return LINGLONG_EWRAP("get ref list: ", refs.error());
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
        auto ret = repo.latestOfRef(appID, "");
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("found app latest version", ret.error());
        }
        version = ret->version;
    }
    // 加载应用
    linglong::package::Ref ref("", channel, appID, version, arch, module);
    auto app = linglong::runtime::App::load(&repo, ref, exec, &ociCli);
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
    auto ret = app->start();
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("start app", ret.error());
    }
    return LINGLONG_OK;
}

auto AppManager::Start(const RunParamOption &paramOption) -> Reply
{
    Reply reply;
    reply.code = -1;
    reply.message = "start method is deprecated";
    return reply;
}

Reply AppManager::Exec(const ExecParamOption &paramOption)
{
    Reply reply;
    auto const &containerID = paramOption.containerID;

    std::string executable = paramOption.cmd[0].toStdString();
    std::vector<std::string> command;
    for (auto cmd : paramOption.cmd.mid(1)) {
        command.push_back(cmd.toStdString());
    }
    ocppi::runtime::ExecOption opt;
    opt.tty = true;
    opt.uid = getuid();
    opt.gid = getgid();
    auto ret = ociCli.exec(containerID.toStdString().c_str(), executable, command, { opt });
    if (!ret.has_value()) {
        auto err = LINGLONG_SERR(ret.error());
        reply.code = STATUS_CODE(kFail);
        reply.message = "exec container failed: " + err.value().message();
        return reply;
    }
    reply.code = STATUS_CODE(kSuccess);
    return reply;
}

Reply AppManager::Stop(const QString &containerId)
{
    Reply reply;
    auto id = containerId.toStdString();
    auto ret = ociCli.kill(id.c_str(), "SIGTERM");
    if (!ret.has_value()) {
        reply.message = "kill container failed, containerId:" + containerId;
        reply.code = STATUS_CODE(kErrorPkgKillFailed);
    } else {
        reply.code = STATUS_CODE(kErrorPkgKillSuccess);
        reply.message = "kill container success, containerId:" + containerId;
    }
    auto err = LINGLONG_SERR(ret.error());
    qInfo() << "kill containerId:" << containerId << ", ret:" << err.value().message();
    return reply;
}

QueryReply AppManager::ListContainer()
{
    QueryReply reply;
    auto ret = ociCli.list();
    if (!ret.has_value()) {
        auto err = LINGLONG_SERR(ret.error());
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
