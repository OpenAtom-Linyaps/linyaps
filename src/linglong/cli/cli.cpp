/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "linglong/package/info.h"
#include "linglong/package/layer_file.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"

#include <nlohmann/json.hpp>

#include <grp.h>
#include <sys/wait.h>

using namespace linglong::utils::error;

namespace linglong::cli {

const char Cli::USAGE[] =
  R"(linglong CLI

A CLI program to run application and manage linglong pagoda and tiers.

Usage:
    ll-cli [--json] --version
    ll-cli [--json] run APP [--no-dbus-proxy] [--dbus-proxy-cfg=PATH] [--] [COMMAND...]
    ll-cli [--json] ps
    ll-cli [--json] exec PAGODA [--working-directory=PATH] [--] COMMAND...
    ll-cli [--json] enter PAGODA [--working-directory=PATH] [--] [COMMAND...]
    ll-cli [--json] kill PAGODA
    ll-cli [--json] [--no-dbus] install TIER...
    ll-cli [--json] uninstall TIER... [--all] [--prune]
    ll-cli [--json] upgrade TIER...
    ll-cli [--json] search [--type=TYPE] TEXT
    ll-cli [--json] [--no-dbus] list [--type=TYPE]
    ll-cli [--json] repo modify [--name=REPO] URL
    ll-cli [--json] repo list
    ll-cli [--json] info LAYER

Arguments:
    APP     Specify the application.
    PAGODA  Specify the pagodas (container).
    TIER    Specify the tier (container layer).
    URL     Specify the new repo URL.
    TEXT    The text used to search tiers.
    LAYER   Specify the layer path

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
    --prune                   Remove application data if the tier is an application and all version of that application has been removed.

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
    repo       Display or modify information of the repository currently using.
    info       Display the information of layer
)";

namespace {
/**
 * @brief 处理安装中断请求
 *
 * @param sig 中断信号
 */
void doIntOperate(int /*sig*/)
{
    // FIXME(black_desk): should use sig

    // 显示光标
    std::cout << "\033[?25h" << std::endl;
    // Fix to 调用jobManager中止下载安装操作
    exit(0);
}

auto parseDataFromProxyCfgFile(const QString &dbusProxyCfgPath) -> Result<DBusProxyConfig>
{
    auto dataFile = QFile(dbusProxyCfgPath);
    dataFile.open(QIODevice::ReadOnly);
    if (dataFile.isOpen()) {
        return LINGLONG_ERR(-1, "proxy config data not found");
    }

    auto data = dataFile.readAll();
    if (data.size() == 0) {
        return LINGLONG_ERR(-1, "failed to read config data");
    }

    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        return LINGLONG_ERR(-1, jsonError.errorString());
    }
    DBusProxyConfig config;
    if (!doc.isNull()) {
        QJsonObject jsonObject = doc.object();
        config.enable = jsonObject.value("enable").toBool();
        config.busType = jsonObject.value("busType").toString();
        config.appId = jsonObject.value("appId").toString();
        config.proxyPath = jsonObject.value("proxyPath").toString();
        config.name = QStringList{ jsonObject.value("name").toString() };
        config.path = QStringList{ jsonObject.value("path").toString() };
        config.interface = QStringList{ jsonObject.value("interface").toString() };
    }
    return config;
}

QList<pid_t> childrenOf(pid_t p)
{
    auto childrenPath = QString("/proc/%1/task/%1/children").arg(p);
    QFile child(childrenPath);
    if (!child.open(QIODevice::ReadOnly)) {
        return {};
    }

    auto data = child.readAll();
    auto slice = QString::fromLatin1(data).split(" ");
    QList<pid_t> pidList;
    for (auto const &str : slice) {
        if (str.isEmpty()) {
            continue;
        }
        pidList.push_back(str.toInt());
    }
    return pidList;
}

int bringDownPermissionsTo(const struct stat &fileStat)
{
    __gid_t newGid[1] = { fileStat.st_gid };

    if (setgroups(1, newGid)) {
        return errno;
    }
    if (setgid(fileStat.st_gid) != 0) {
        return errno;
    }
    if (setegid(fileStat.st_gid) != 0) {
        return errno;
    }
    if (setuid(fileStat.st_uid) != 0) {
        return errno;
    }
    if (seteuid(fileStat.st_uid) != 0) {
        return errno;
    }
    return 0;
}

int execArgs(const std::vector<std::string> &args, const std::vector<std::string> &envStrVector)
{
    std::vector<char *> argVec(args.size() + 1);
    std::transform(args.begin(), args.end(), argVec.begin(), [](const std::string &str) -> char * {
        return const_cast<char *>(str.c_str());
    });
    argVec.push_back(nullptr);

    auto command = argVec.data();

    std::vector<char *> envVec(envStrVector.size() + 1);
    std::transform(envStrVector.begin(),
                   envStrVector.end(),
                   envVec.begin(),
                   [](const std::string &str) -> char * {
                       return const_cast<char *>(str.c_str());
                   });
    envVec.push_back(nullptr);

    auto env = envVec.data();

    return execve(command[0], &command[0], &env[0]);
}

int getChildBoxPID(pid_t pid)
{
    auto children = childrenOf(pid);
    if (children.isEmpty()) {
        return 0;
    }

    for (const auto &child : children) {
        auto exe = QFile(QString("/proc/%1/exe").arg(child)).symLinkTarget();
        if (exe != "/usr/bin/ll-box") {
            continue;
        }
        return child;
    }

    return 0;
}

int namespaceEnter(pid_t pid, const QString &id)
{
    int ret;
    struct stat fileStat = {};

    auto childBox = getChildBoxPID(pid);
    if (childBox == 0) {
        qDebug() << "no ll-box process found in child of" << pid;
        return -1;
    }
    childBox = getChildBoxPID(childBox);
    if (childBox == 0) {
        qDebug() << "no ll-box process found in child of" << pid;
        return -1;
    }

    auto target = QString::number(childBox).toUtf8();

    auto sysenv = QProcessEnvironment::systemEnvironment();

    QFile envFile(QString("/run/user/%1/linglong/%2/env").arg(getuid()).arg(id));
    envFile.open(QIODevice::ReadOnly);
    if (envFile.isOpen()) {
        QTextStream in(&envFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            auto name = line.left(line.indexOf('='));
            auto value = line.mid(line.indexOf('=') + 1);
            sysenv.insert(name, value);
        }
        envFile.close();
    } else {
        qWarning() << "Failed to open env file, not init environment variables.";
    }

    auto nsenter = QProcess();
    nsenter.setProcessEnvironment(sysenv);
    nsenter.setInputChannelMode(QProcess::ForwardedInputChannel);
    nsenter.setProcessChannelMode(QProcess::ForwardedChannels);
    nsenter.setProgram("nsenter");
    nsenter.setArguments({ "-t",
                           target,
                           "-U",
                           "-m",
                           "-p",
                           "--preserve-credentials",
                           "--",
                           "env",
                           R"(debian_chroot=LINGLONG)",
                           "bash" });

    nsenter.start();
    nsenter.waitForFinished(-1);

    return nsenter.exitCode();
}

} // namespace

Cli::Cli(Printer &printer,
         linglong::service::AppManager &appMan,
         linglong::api::dbus::v1::PackageManager &pkgMan)
    : printer(printer)
    , appMan(appMan)
    , pkgMan(pkgMan)
{
}

// TODD(wurongjie) 自动将输入转为container id
utils::error::Result<QString> Cli::getContainerID(std::map<std::string, docopt::value> &args)
{
    QString containerId;
    if (args["PAGODA"].isString()) {
        containerId = QString::fromStdString(args["PAGODA"].asString());
    }

    if (args["APP"].isString()) {
        containerId = QString::fromStdString(args["APP"].asString());
    }

    if (containerId.isEmpty()) {
        return LINGLONG_ERR(400, "canot get container id");
    }
    return containerId;
}

int Cli::run(std::map<std::string, docopt::value> &args)
{
    const auto appId = QString::fromStdString(args["APP"].asString());
    if (appId.isEmpty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "Application ID is required.").value());
        return -1;
    }

    linglong::service::RunParamOption paramOption;

    // FIXME(black_desk): It seems that paramOption should directly take a ref.
    linglong::package::Ref ref(appId);
    paramOption.appId = ref.appId;
    paramOption.version = ref.version;

    auto command = args["COMMAND"].asStringList();
    if (!command.empty()) {
        for (const auto &arg : command) {
            paramOption.exec.push_back(QString::fromStdString(arg));
        }
    }

    // 获取用户环境变量
    QStringList envList = utils::command::getUserEnv(utils::command::envList);
    if (!envList.isEmpty()) {
        paramOption.appEnv = envList;
    }
    // 判断是否设置了no-proxy参数
    paramOption.noDbusProxy = args["--no-dbus-proxy"].asBool();

    if (args["--dbus-proxy-cfg"].isString()) {
        auto dbusProxyCfg = QString::fromStdString(args["--dbus-proxy-cfg"].asString());
        if (!dbusProxyCfg.isEmpty()) {
            auto data = parseDataFromProxyCfgFile(dbusProxyCfg);
            if (!data.has_value()) {
                printer.printErr(LINGLONG_ERR(-2, "get empty data from cfg file").value());
                return -1;
            }
            // TODO(linxin): parse dbus filter info from config path
            paramOption.busType = "session";
            paramOption.filterName = data->name[0];
            paramOption.filterPath = data->path[0];
            paramOption.filterInterface = data->interface[0];
        }
    }
    qDebug() << "send param" << paramOption.appId << "to service";
    auto ret = this->appMan.Run(paramOption);
    if (!ret.has_value()) {
        this->printer.printErr(ret.error());
        return -1;
    }
    return 0;
}

int Cli::exec(std::map<std::string, docopt::value> &args)
{
    auto ret = getContainerID(args);
    if (!ret.has_value()) {
        this->printer.printErr(ret.error());
        return -1;
    }
    service::ExecParamOption opt;
    opt.containerID = *ret;
    if (args["COMMAND"].isStringList()) {
        const auto &command = args["COMMAND"].asStringList();
        for (const auto &arg : command) {
            opt.cmd.push_back(QString::fromStdString(arg));
        }
    }
    appMan.Exec(opt);
    return 0;
}

int Cli::enter(std::map<std::string, docopt::value> &args)
{
    QString containerId;

    containerId = QString::fromStdString(args["PAGODA"].asString());

    if (args["--working-directory"].isBool() && args["--working-directory"].asBool()) {
        qWarning() << "--working-directory is not supported yet";
    }

    if (args["COMMAND"].isStringList() && !args["COMMAND"].asStringList().empty()) {
        qWarning() << "COMMAND is not supported yet";
    }

    if (containerId.isEmpty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to get container id").value());
        return -1;
    }

    auto result = this->appMan.ListContainer().result;

    QList<QSharedPointer<Container>> containerList;
    const auto doc = QJsonDocument::fromJson(result.toUtf8(), nullptr);
    if (!doc.isArray()) {
        this->printer.printErr(
          LINGLONG_ERR(-1, "container list get from server is not a list").value());
        return -1;
    }

    pid_t pid = 0;
    QString id = "";

    for (const auto item : doc.array()) {
        const auto str = QString(QJsonDocument(item.toObject()).toJson());
        QSharedPointer<Container> container(linglong::util::loadJsonString<Container>(str));
        if (container->id == containerId) {
            pid = container->pid;
            id = container->id;
            break;
        }
        if (container->packageName.contains(containerId)) {
            pid = container->pid;
            id = container->id;
            break;
        }
    }

    if (pid == 0) {
        this->printer.printErr(LINGLONG_ERR(-1, "no such container").value());
        return -1;
    }

    int reply = namespaceEnter(pid, id);
    if (reply == -1) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to enter namespace").value());
        return -1;
    }

    return 0;
}

int Cli::ps(std::map<std::string, docopt::value> &args)
{
    auto ret = this->appMan.ListContainer();
    const auto replyString = ret.result;

    QList<QSharedPointer<Container>> containerList;
    const auto doc = QJsonDocument::fromJson(replyString.toUtf8(), nullptr);
    if (!doc.isArray()) {
        this->printer.printErr(
          LINGLONG_ERR(-1, "container list get from server is not a list").value());
        return -1;
    }

    for (const auto container : doc.array()) {
        const auto str = QString(QJsonDocument(container.toObject()).toJson());
        QSharedPointer<Container> con(linglong::util::loadJsonString<Container>(str));
        containerList.push_back(con);
    }
    this->printer.printContainers(containerList);
    return 0;
}

int Cli::kill(std::map<std::string, docopt::value> &args)
{
    auto ret = getContainerID(args);

    if (!ret.has_value()) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto reply = this->appMan.Stop(*ret);
    if (reply.code != STATUS_CODE(kErrorPkgKillSuccess)) {
        this->printer.printErr(LINGLONG_ERR(reply.code, reply.message).value());
        return -1;
    }

    this->printer.printReply(reply);
    return 0;
}

int Cli::install(std::map<std::string, docopt::value> &args)
{
    // 收到中断信号后恢复操作
    signal(SIGINT, doIntOperate);
    // 设置 24 h超时
    this->pkgMan.setTimeout(1000 * 60 * 60 * 24);
    // appId format: org.deepin.calculator/1.2.6 in multi-version
    linglong::service::InstallParamOption installParamOption;
    auto tiers = args["TIER"].asStringList();
    if (tiers.empty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to get app id").value());
        return -1;
    }
    for (auto &tier : tiers) {
        linglong::service::Reply reply;
        // if specify a layer instead of an appID
        const auto layerFile = package::LayerFile::openLayer(QString::fromStdString(tier));
        if (layerFile.has_value()) {
            (*layerFile)->setCleanStatus(false);

            QFileInfo fileInfo(*(*layerFile));
            installParamOption.layerPath = fileInfo.absoluteFilePath();
            installParamOption.layerName = fileInfo.fileName();
            layerFile->data()->seek(0);
            QDBusUnixFileDescriptor dbusFileDescriptor(layerFile->data()->handle());
            QDBusPendingReply<linglong::service::Reply> dbusReply =
              this->pkgMan.InstallLayerFD(dbusFileDescriptor);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            this->printer.printReply(reply);
            continue;
        }

        // if specify an appID
        linglong::package::Ref ref(QString::fromStdString(tier));
        // 增加 channel/module
        installParamOption.channel = ref.channel;
        installParamOption.appModule = ref.module;
        installParamOption.appId = ref.appId;
        if (ref.version != "latest") {
            installParamOption.version = ref.version;
        }
        installParamOption.arch = ref.arch;

        qInfo().noquote() << "install" << ref.appId << ", please wait a few minutes...";

        QDBusPendingReply<linglong::service::Reply> dbusReply =
          this->pkgMan.Install(installParamOption);
        dbusReply.waitForFinished();
        reply = dbusReply.value();
        qDebug() << reply.message;
        QThread::sleep(1);
        // 查询一次进度
        dbusReply = this->pkgMan.GetDownloadStatus(installParamOption, 0);
        dbusReply.waitForFinished();
        reply = dbusReply.value();
        bool disProgress = false;
        // 隐藏光标
        std::cout << "\033[?25l";
        while (reply.code == STATUS_CODE(kPkgInstalling)) {
            std::cout << "\r\33[K" << reply.message.toStdString();
            std::cout.flush();
            QThread::sleep(1);
            dbusReply = this->pkgMan.GetDownloadStatus(installParamOption, 0);
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
            this->printer.printErr(LINGLONG_ERR(reply.code, reply.message).value());
            return -1;
        }

        this->printer.printReply(reply);
    }
    return 0;
}

int Cli::upgrade(std::map<std::string, docopt::value> &args)
{
    linglong::service::ParamOption paramOption;
    auto tiers = args["TIER"].asStringList();

    if (tiers.empty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to get app id").value());
        return -1;
    }

    for (auto &tier : tiers) {
        linglong::package::Ref ref(QString::fromStdString(tier));
        paramOption.arch = linglong::util::hostArch();
        if (!ref.version.isEmpty() && ref.version != "latest") {
            paramOption.version = ref.version;
        }
        paramOption.appId = ref.appId;
        // 增加 channel/module
        paramOption.channel = ref.channel;
        paramOption.appModule = ref.module;
        pkgMan.setTimeout(1000 * 60 * 60 * 24);
        qInfo().noquote() << "upgrade" << paramOption.appId << ", please wait a few minutes...";
        QDBusPendingReply<linglong::service::Reply> dbusReply = pkgMan.Update(paramOption);
        dbusReply.waitForFinished();
        linglong::service::Reply reply;
        reply = dbusReply.value();
        if (reply.code == STATUS_CODE(kPkgUpdating)) {
            signal(SIGINT, doIntOperate);
            QThread::sleep(1);
            dbusReply = pkgMan.GetDownloadStatus(paramOption, 1);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            bool disProgress = false;
            // 隐藏光标
            std::cout << "\033[?25l";
            while (reply.code == STATUS_CODE(kPkgUpdating)) {
                std::cout << "\r\33[K" << reply.message.toStdString();
                std::cout.flush();
                QThread::sleep(1);
                dbusReply = pkgMan.GetDownloadStatus(paramOption, 1);
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
            this->printer.printErr(LINGLONG_ERR(-1, reply.message).value());
            return -1;
        }
        this->printer.printReply(reply);
    }

    return 0;
}

int Cli::search(std::map<std::string, docopt::value> &args)
{
    linglong::service::QueryParamOption paramOption;
    QString appId;
    if (args["TEXT"].isString()) {
        appId = QString::fromStdString(args["TEXT"].asString());
    }
    if (appId.isEmpty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "faied to get app id").value());
        return -1;
    }
    paramOption.force = true;
    paramOption.appId = appId;
    this->pkgMan.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = this->pkgMan.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kErrorPkgQuerySuccess)) {
        this->printer.printErr(LINGLONG_ERR(reply.code, reply.message).value());
        return -1;
    }

    auto [appMetaInfoList, err] =
      linglong::util::fromJSON<QList<QSharedPointer<linglong::package::AppMetaInfo>>>(
        reply.result.toLocal8Bit());
    if (err) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to parse json reply").value());
        return -1;
    }

    if (appMetaInfoList.empty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "app not found in repo").value());
        return -1;
    }

    this->printer.printAppMetaInfos(appMetaInfoList);
    return 0;
}

int Cli::uninstall(std::map<std::string, docopt::value> &args)
{
    this->pkgMan.setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::Reply> dbusReply;
    linglong::service::UninstallParamOption paramOption;

    auto tiers = args["TIER"].asStringList();

    for (auto &tier : tiers) {
        linglong::package::Ref ref(QString::fromStdString(tier));
        if (!ref.version.isEmpty() && ref.version != "latest") {
            paramOption.version = ref.version;
        }
        paramOption.appId = ref.appId;
        paramOption.channel = ref.channel;
        paramOption.appModule = ref.module;
        paramOption.delAppData = args["--prune"].asBool();
        linglong::service::Reply reply;
        qInfo().noquote() << "uninstall" << paramOption.appId << ", please wait a few minutes...";
        paramOption.delAllVersion = args["--all"].asBool();
        dbusReply = this->pkgMan.Uninstall(paramOption);
        dbusReply.waitForFinished();
        reply = dbusReply.value();

        if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
            this->printer.printErr(LINGLONG_ERR(reply.code, reply.message).value());
            return -1;
        }
        this->printer.printReply(reply);
    }

    return 0;
}

int Cli::list(std::map<std::string, docopt::value> &args)
{
    linglong::service::QueryParamOption paramOption;
    QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList;
    linglong::service::QueryReply reply;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = this->pkgMan.Query(paramOption);
    // 默认超时时间为25s
    dbusReply.waitForFinished();
    reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, appMetaInfoList);
    this->printer.printAppMetaInfos(appMetaInfoList);
    return 0;
}

int Cli::repo(std::map<std::string, docopt::value> &args)
{
    if (!args["modify"].asBool()) {
        auto dbusReply = this->pkgMan.getRepoInfo();
        dbusReply.waitForFinished();
        auto reply = dbusReply.value();

        this->printer.printQueryReply(reply);
        return 0;
    }

    QString name;
    QString url;
    if (args["--name"].isString()) {
        name = QString::fromStdString(args["--name"].asString());
    }
    if (args["URL"].isString()) {
        url = QString::fromStdString(args["URL"].asString());
    }
    linglong::service::Reply reply;
    auto dbusReply = this->pkgMan.ModifyRepo(name, url);
    dbusReply.waitForFinished();
    reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kErrorModifyRepoSuccess)) {
        this->printer.printErr(LINGLONG_ERR(reply.code, reply.message).value());
        return -1;
    }
    this->printer.printReply(reply);
    return 0;
}

int Cli::info(std::map<std::string, docopt::value> &args)
{
    QString layerPath;
    if (args["LAYER"].isString()) {
        layerPath = QString::fromStdString(args["LAYER"].asString());
    }

    if (layerPath.isEmpty()) {
        this->printer.printErr(LINGLONG_ERR(-1, "failed to get layer path").value());
        return -1;
    }

    const auto layerFile = package::LayerFile::openLayer(layerPath);

    if (!layerFile.has_value()) {
        this->printer.printErr(layerFile.error());
        return -1;
    }

    (*layerFile)->setCleanStatus(false);

    const auto layerInfo = (*layerFile)->layerFileInfo();
    if (!layerInfo.has_value()) {
        this->printer.printErr(layerInfo.error());
        return -1;
    }

    const auto rawData = QByteArray::fromStdString(nlohmann::json((*layerInfo).info).dump());
    auto [pkgInfo, err] = util::fromJSON<QSharedPointer<package::Info>>(rawData);

    if (err) {
        this->printer.printErr(
          LINGLONG_ERR(err.code(), "failed to parse package::Info. " + err.message()).value());
        return -1;
    }

    this->printer.printLayerInfo(pkgInfo);
    return 0;
}

} // namespace linglong::cli
