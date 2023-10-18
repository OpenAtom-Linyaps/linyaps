/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/api/v1/dbus/app_manager1.h"
#include "linglong/api/v1/dbus/package_manager1.h"
#include "linglong/package/package.h"
#include "linglong/package/ref.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/service/app_manager.h"
#include "linglong/util/app_status.h"
#include "linglong/util/command_helper.h"
#include "linglong/util/env.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/global/initialize.h"

#include <docopt.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>
#include <fstream>
#include <iostream>

static qint64 systemHelperPid = -1;

static const char USAGE[] =
  R"(linglong CLI

A CLI program to run application and manage linglong pagoda and tiers.

    Usage:
        ll-cli [--json] --version
        ll-cli [--json] run APP [--no-dbus-proxy] [--dbus-proxy-cfg=PATH] [--] [COMMAND...]
        ll-cli [--json] ps
        ll-cli [--json] exec (APP | PAGODA) [--working-directory=PATH] [--] COMMAND...
        ll-cli [--json] enter (APP | PAGODA) [--working-directory=PATH] [--] [COMMAND...]
        ll-cli [--json] kill (APP | PAGODA)
        ll-cli [--json] [--no-dbus] install TIER...
        ll-cli [--json] uninstall TIER... [--all] [--prune]
        ll-cli [--json] upgrade TIER...
        ll-cli [--json] search [--type=TYPE] TEXT
        ll-cli [--json] [--no-dbus] list [--type=TYPE]
        ll-cli [--json] repo [modify [--name=REPO] URL]

    Arguments:
        APP     Specify the application.
        PAGODA  Specify the pagodas (container).
        TIER    Specify the tier (container layer).
        URL     Specify the new repo URL.
        TEXT    The text used to search tiers.

    Options:
        -h --help                 Show this screen.
        --version                 Show version.
        --json                    Use json to output command result.
        --no-dbus                 Use peer to peer DBus, this is used only in case that DBus daemon is not available.
        --no-dbus-proxy           Do not enable linglong-dbus-proxy.
        --dbus-proxy-cfg=PATH     Path of config of linglong-dbus-proxy.
        --working-directory=PATH  Specify working directory.
        --type=TYPE               Filter result with tiers type. One of "lib", "app" or "dev". [default: app]
        --state=STATE             Filter result with the tiers install state. Should be "local" or "remote". [default: local]
        --prune                   Remove application data if the tier is an application and all version of that application has benn removed.

    Subcommands:
        run        Run an application.
        ps         List all pagodas.
        exec       Execute command in a pagoda.
        enter      Enter a pagoda.
        kill       Stop applications and remove the pagoda.
        install    Install tier(s).
        uninstall  Uninstall tier(s).
        upgrade    Upgrade tier(s).
        search     Search for tiers.
        list       List known tiers.
        repo       Disply or modify infomation of the repository currently using.
)";

/**
 * @brief 统计字符串中中文字符的个数
 *
 * @param name 软件包名称
 * @return int 中文字符个数
 */
static int getUnicodeNum(const QString &name)
{
    int num = 0;
    int count = name.count();
    for (int i = 0; i < count; i++) {
        QChar ch = name.at(i);
        ushort decode = ch.unicode();
        if (decode >= 0x4E00 && decode <= 0x9FA5) {
            num++;
        }
    }
    return num;
}

/**
 * @brief 处理安装中断请求
 *
 * @param sig 中断信号
 */
static void doIntOperate(int /*sig*/)
{
    // FIXME(black_desk): should use sig

    // 显示光标
    std::cout << "\033[?25h" << std::endl;
    // Fix to 调用jobManager中止下载安装操作
    exit(0);
}

static void handleOnExit(int, void *)
{
    if (systemHelperPid != -1) {
        kill(systemHelperPid, SIGTERM);
    }
}

/**
 * @brief 输出软件包的查询结果
 *
 * @param appMetaInfoList 软件包元信息列表
 *
 */
static void printAppInfo(QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList)
{
    if (appMetaInfoList.size() > 0) {
        qInfo("\033[1m\033[38;5;214m%-32s%-32s%-16s%-12s%-16s%-12s%-s\033[0m",
              qUtf8Printable("appId"),
              qUtf8Printable("name"),
              qUtf8Printable("version"),
              qUtf8Printable("arch"),
              qUtf8Printable("channel"),
              qUtf8Printable("module"),
              qUtf8Printable("description"));
        for (const auto &it : appMetaInfoList) {
            QString simpleDescription = it->description.trimmed();
            if (simpleDescription.length() > 56) {
                simpleDescription = it->description.trimmed().left(53) + "...";
            }
            QString appId = it->appId.trimmed();
            QString name = it->name.trimmed();
            if (name.length() > 32) {
                name = it->name.trimmed().left(29) + "...";
            }
            if (appId.length() > 32) {
                name.push_front(" ");
            }
            int count = getUnicodeNum(name);
            int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
            qInfo().noquote() << QString("%1%2%3%4%5%6%7")
                                   .arg(appId, -32, QLatin1Char(' '))
                                   .arg(name, count - 32, QLatin1Char(' '))
                                   .arg(it->version.trimmed(), -16, QLatin1Char(' '))
                                   .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
                                   .arg(it->channel.trimmed(), -16, QLatin1Char(' '))
                                   .arg(it->module.trimmed(), -12, QLatin1Char(' '))
                                   .arg(simpleDescription, -length, QLatin1Char(' '));
        }
    } else {
        qInfo().noquote() << "app not found in repo";
    }
}

static void startDaemon(QString program, QStringList args = {}, qint64 *pid = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setStandardOutputFile("/dev/null");
    process.setStandardErrorFile("/dev/null");
    process.setArguments(args);
    process.startDetached(pid);
}

// Run an application.
static int subcommandRun(std::map<std::string, docopt::value> &args)
{
    const auto appId = QString::fromStdString(args["APP"].asString());
    if (appId.isEmpty()) {
        return -1;
    }

    linglong::service::RunParamOption paramOption;

    // FIXME(black_desk): It seems that paramOption should directly take a ref.
    linglong::package::Ref ref(appId);
    paramOption.appId = ref.appId;
    paramOption.version = ref.version;

    auto command = args["COMMAND"].asStringList();
    for (const auto &arg : command) {
        paramOption.exec.push_back(QString::fromStdString(arg));
    }

    // 获取用户环境变量
    QStringList envList = COMMAND_HELPER->getUserEnv(linglong::util::envList);
    if (!envList.isEmpty()) {
        paramOption.appEnv = envList;
    }

    // 判断是否设置了no-proxy参数
    paramOption.noDbusProxy = args["--no-dbus-proxy"].asBool();

    auto dbusProxyCfg = args["--dbus-proxy-cfg"].asString();
    if (!dbusProxyCfg.empty()) {
        // TODO(linxin): parse dbus filter info from config path
        // paramOption.busType = "session";
        // paramOption.filterName = parser.value(optNameFilter);
        // paramOption.filterPath = parser.value(optPathFilter);
        // paramOption.filterInterface = parser.value(optInterfaceFilter);
    }

    // TODO: ll-cli 进沙箱环境

    auto appManager = linglong::api::v1::dbus::AppManager1("org.deepin.linglong.AppManager",
                                                           "/org/deepin/linglong/AppManager",
                                                           QDBusConnection::sessionBus());
    qDebug() << "send param" << paramOption.appId << "to service";
    auto dbusReply = appManager.Start(paramOption);
    dbusReply.waitForFinished();
    auto reply = dbusReply.value();
    if (reply.code != 0) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }
    return 0;
}

// Exec new command in an existing pagoda.
static int subcommandExec(std::map<std::string, docopt::value> &args)
{
    const auto containerId = QString::fromStdString(args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        return -1;
    }

    linglong::service::ExecParamOption execOption;
    execOption.containerID = containerId;

    auto &command = args["COMMAND"].asStringList();
    for (const auto &arg : command) {
        execOption.cmd.push_back(QString::fromStdString(arg));
    }

    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());
    const auto dbusReply = appManager.Exec(execOption);
    const auto reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kSuccess)) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }
    return 0;
}

// Enter an existing pagoda, run an interactive bash shell in that pagoda.
static int subcommandEnter(std::map<std::string, docopt::value> &args)
{
    const auto containerId = QString::fromStdString(args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        return -1;
    }

    const auto cmd = QString::fromStdString(args["COMMAND"].asString());
    if (cmd.isEmpty()) {
        return -1;
    }

    const auto pid = containerId.toInt();

    return COMMAND_HELPER->namespaceEnter(pid, QStringList{ cmd });
}

// List all existing pagodas.
static int subcommandPs(std::map<std::string, docopt::value> &args)
{
    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());
    const auto outputFormat = args["--json"].asBool() ? "json" : "nojson";
    const auto replyString = appManager.ListContainer().value().result;

    QList<QSharedPointer<Container>> containerList;
    const auto doc = QJsonDocument::fromJson(replyString.toUtf8(), nullptr);
    if (doc.isArray()) {
        for (const auto container : doc.array()) {
            const auto str = QString(QJsonDocument(container.toObject()).toJson());
            QSharedPointer<Container> con(linglong::util::loadJsonString<Container>(str));
            containerList.push_back(con);
        }
    }
    COMMAND_HELPER->showContainer(containerList, outputFormat);
    return 0;
}

// Stop a pagoda.
static int subcommandKill(std::map<std::string, docopt::value> &args)
{
    const auto containerId = QString::fromStdString(args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        return -1;
    }
    // TODO: show kill result
    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());
    QDBusPendingReply<linglong::service::Reply> dbusReply = appManager.Stop(containerId);
    dbusReply.waitForFinished();
    linglong::service::Reply reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kErrorPkgKillSuccess)) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }

    qInfo().noquote() << reply.message;
    return 0;
}

// Install package.
static int subcommandInstall(std::map<std::string, docopt::value> &args)
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    // 收到中断信号后恢复操作
    signal(SIGINT, doIntOperate);
    // 设置 24 h超时
    sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
    // appId format: org.deepin.calculator/1.2.6 in multi-version
    linglong::service::InstallParamOption installParamOption;
    const auto appId = QString::fromStdString(args["APP"].asString());
    linglong::package::Ref ref(appId);
    // 增加 channel/module
    installParamOption.channel = ref.channel;
    installParamOption.appModule = ref.module;
    installParamOption.appId = ref.appId;
    installParamOption.version = ref.version;
    installParamOption.arch = ref.arch;

    linglong::service::Reply reply;
    qInfo().noquote() << "install" << ref.appId << ", please wait a few minutes...";
    if (!args["--no-dbus"].asBool()) {
        QDBusPendingReply<linglong::service::Reply> dbusReply =
          sysPackageManager.Install(installParamOption);
        dbusReply.waitForFinished();
        reply = dbusReply.value();
        QThread::sleep(1);
        // 查询一次进度
        dbusReply = sysPackageManager.GetDownloadStatus(installParamOption, 0);
        dbusReply.waitForFinished();
        reply = dbusReply.value();
        bool disProgress = false;
        // 隐藏光标
        std::cout << "\033[?25l";
        while (reply.code == STATUS_CODE(kPkgInstalling)) {
            std::cout << "\r\33[K" << reply.message.toStdString();
            std::cout.flush();
            QThread::sleep(1);
            dbusReply = sysPackageManager.GetDownloadStatus(installParamOption, 0);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            disProgress = true;
        }
        // 显示光标
        std::cout << "\033[?25h";
        if (disProgress) {
            std::cout << std::endl;
        }
        if (reply.code != STATUS_CODE(kPkgInstallSuccess)) {
            if (reply.message.isEmpty()) {
                reply.message = "unknown err";
                reply.code = -1;
            }
            qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
            return -1;
        } else {
            qInfo().noquote() << "message:" << reply.message;
        }
    } else {
        linglong::service::PackageManager packageManager;
        packageManager.setNoDBusMode(true);
        reply = packageManager.Install(installParamOption);
        packageManager.pool->waitForDone(-1);
        qInfo().noquote() << "install " << installParamOption.appId << " done";
    }
    return 0;
}

// Upgrade package, which means install the new version and remove old version of that package.
static int subcommandUpgrade(std::map<std::string, docopt::value> &args)
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());
    linglong::service::ParamOption paramOption;
    const auto appId = QString::fromStdString(args["APP"].asString());
    if (appId.isEmpty()) {
        // parser.showHelp(-1);
        return -1;
    }
    linglong::package::Ref ref(appId);
    paramOption.arch = linglong::util::hostArch();
    paramOption.version = ref.version;
    paramOption.appId = ref.appId;
    // 增加 channel/module
    paramOption.channel = ref.channel;
    paramOption.appModule = ref.module;
    sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
    qInfo().noquote() << "update" << paramOption.appId << ", please wait a few minutes...";
    QDBusPendingReply<linglong::service::Reply> dbusReply = sysPackageManager.Update(paramOption);
    dbusReply.waitForFinished();
    linglong::service::Reply reply;
    reply = dbusReply.value();
    if (reply.code == STATUS_CODE(kPkgUpdating)) {
        signal(SIGINT, doIntOperate);
        QThread::sleep(1);
        dbusReply = sysPackageManager.GetDownloadStatus(paramOption, 1);
        dbusReply.waitForFinished();
        reply = dbusReply.value();
        bool disProgress = false;
        // 隐藏光标
        std::cout << "\033[?25l";
        while (reply.code == STATUS_CODE(kPkgUpdating)) {
            std::cout << "\r\33[K" << reply.message.toStdString();
            std::cout.flush();
            QThread::sleep(1);
            dbusReply = sysPackageManager.GetDownloadStatus(paramOption, 1);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            disProgress = true;
        }
        // 显示光标
        std::cout << "\033[?25h";
        if (disProgress) {
            std::cout << std::endl;
        }
    }
    if (reply.code != STATUS_CODE(kErrorPkgUpdateSuccess)) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }
    qInfo().noquote() << "message:" << reply.message;
    return 0;
}

// Search tiers online.
static int subcommandSearch(std::map<std::string, docopt::value> &args)
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    linglong::service::QueryParamOption paramOption;
    const auto appId = QString::fromStdString(args["APP"].asString());
    if (appId.isEmpty()) {
        return -1;
    }
    paramOption.force = true;
    paramOption.appId = appId;
    sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::QueryReply> dbusReply =
      sysPackageManager.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kErrorPkgQuerySuccess)) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }

    auto [appMetaInfoList, err] =
      linglong::util::fromJSON<QList<QSharedPointer<linglong::package::AppMetaInfo>>>(
        reply.result.toLocal8Bit());
    if (err) {
        qCritical() << "parse json reply failed:" << err;
        return -1;
    }

    printAppInfo(appMetaInfoList);
    return 0;
}

// Uninstall a tier.
static int subcommandUninstall(std::map<std::string, docopt::value> &args)
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::Reply> dbusReply;
    linglong::service::UninstallParamOption paramOption;
    // appId format: org.deepin.calculator/1.2.6 in multi-version
    // QStringList appInfoList = appInfo.split("/");
    const QString appId = QString::fromStdString(args["APP"].asString());
    linglong::package::Ref ref(appId);
    paramOption.version = ref.version;
    paramOption.appId = appId;
    paramOption.channel = ref.channel;
    paramOption.appModule = ref.module;
    paramOption.delAppData = args["--prune"].asBool();
    linglong::service::Reply reply;
    qInfo().noquote() << "uninstall" << paramOption.appId << ", please wait a few minutes...";
    paramOption.delAllVersion = args["--all"].asBool();
    if (args["--no-dbus"].asBool()) {
        linglong::service::PackageManager packageManager;
        packageManager.setNoDBusMode(true);
        reply = packageManager.Uninstall(paramOption);
        if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
            qInfo().noquote() << "message: " << reply.message << ", errcode:" << reply.code;
            return -1;
        } else {
            qInfo().noquote() << "uninstall " << paramOption.appId << " success";
        }
        return 0;
    }
    dbusReply = sysPackageManager.Uninstall(paramOption);
    dbusReply.waitForFinished();
    reply = dbusReply.value();

    if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
        qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
        return -1;
    }
    qInfo().noquote() << "message:" << reply.message;
    return 0;
}

// List all installed tiers.
static int subcommandList(std::map<std::string, docopt::value> &args)
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    linglong::service::QueryParamOption paramOption;
    linglong::package::Ref ref(QString::fromStdString(args["APP"].asString()));
    paramOption.appId = ref.appId;
    QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList;
    linglong::service::QueryReply reply;
    if (args["--no-dbus"].asBool()) {
        linglong::service::PackageManager packageManager;
        reply = packageManager.Query(paramOption);
    } else {
        QDBusPendingReply<linglong::service::QueryReply> dbusReply =
          sysPackageManager.Query(paramOption);
        // 默认超时时间为25s
        dbusReply.waitForFinished();
        reply = dbusReply.value();
    }
    linglong::util::getAppMetaInfoListByJson(reply.result, appMetaInfoList);
    printAppInfo(appMetaInfoList);
    return 0;
}

// Modify linglong tier repo settings.
static int subcommandRepo(std::map<std::string, docopt::value> &args)
{

    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    if (args["modify"].asBool()) {
        const auto name = QString::fromStdString(args["--name"].asString());
        const auto url = QString::fromStdString(args["URL"].asString());
        linglong::service::Reply reply;
        if (!args["-no-dbus-proxy"].asBool()) {
            QDBusPendingReply<linglong::service::Reply> dbusReply =
              sysPackageManager.ModifyRepo(name, url);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
        } else {
            linglong::service::PackageManager packageManager;
            reply = packageManager.ModifyRepo(name, url);
        }
        if (reply.code != STATUS_CODE(kErrorModifyRepoSuccess)) {
            qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
            return -1;
        }
        qInfo().noquote() << reply.message;
        return 0;
    }

    if (args["list"].asBool()) {
        linglong::service::QueryReply reply;
        QDBusPendingReply<linglong::service::QueryReply> dbusReply =
          sysPackageManager.getRepoInfo();
        dbusReply.waitForFinished();
        reply = dbusReply.value();

        qInfo().noquote() << QString("%1%2").arg("Name", -10).arg("Url");
        qInfo().noquote() << QString("%1%2").arg(reply.message, -10).arg(reply.result);
        return 0;
    }

    qCritical() << "This should never happen.";
    return -1;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    std::map<std::string, docopt::value> args =
      docopt::docopt(USAGE,
                     { argv + 1, argv + argc },
                     true,                  // show help if requested
                     "linglong CLI 1.4.0"); // version string

    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());

    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    auto systemHelperDBusConnection = QDBusConnection::systemBus();
    const auto systemHelperAddress = QString("unix:path=/run/linglong_system_helper_socket");

    if (args["--no-dbus-proxy"]) {
        on_exit(handleOnExit, nullptr);
        // NOTE: isConnected will NOT RETRY
        // NOTE: name cannot be duplicate
        systemHelperDBusConnection =
          QDBusConnection::connectToPeer(systemHelperAddress, "ll-system-helper-1");
        if (!systemHelperDBusConnection.isConnected()) {
            startDaemon("ll-system-helper", { "--bus=" + systemHelperAddress }, &systemHelperPid);
            QThread::sleep(1);
            systemHelperDBusConnection =
              QDBusConnection::connectToPeer(systemHelperAddress, "ll-system-helper");
            if (!systemHelperDBusConnection.isConnected()) {
                qCritical() << "failed to start ll-system-helper";
                exit(-1);
            }
        }
        setenv("LINGLONG_SYSTEM_HELPER_ADDRESS", systemHelperAddress.toStdString().c_str(), true);
    }

    QMap<QString, std::function<int(std::map<std::string, docopt::value> &)>> subcommandMap = {
        { "run", subcommandRun },
        { "exec", subcommandExec },
        { "enter", subcommandEnter },
        { "ps", subcommandPs },
        { "kill", subcommandKill },
        { "install", subcommandInstall },
        { "upgrade", subcommandUpgrade },
        { "search", subcommandSearch },
        { "uninstall", subcommandUninstall },
        { "list", subcommandList },
        { "repo", subcommandRepo },
    };

    for (const auto &subcommand : subcommandMap.keys()) {
        if (args[subcommand.toStdString()].asBool() == true) {
            return subcommandMap[subcommand](args);
        }
    }
}
