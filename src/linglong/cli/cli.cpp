#include "cli.h"
#include "linglong/utils/error/error.h"

using namespace linglong::utils::error;

Cli::Cli(std::map<std::string, docopt::value> &args, std::unique_ptr<Printer> printer): 
m_args(args),
p(std::move(printer))
{

};

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

static auto parseDataFromProxyCfgFile(const QString &dbusProxyCfgPath) -> Result<DBusProxyConfig>
{
    auto dataFile = QFile(dbusProxyCfgPath);
    dataFile.open(QIODevice::ReadOnly);
    if (dataFile.isOpen()) {
        return EWrap("proxy config data not found", nullptr);
    }

    auto data = dataFile.readAll();
    if (data.size() == 0){
        return EWrap("failed to read config data", nullptr);
    }

    QJsonParseError jsonError;
    auto doc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error) {
        return Err(-1, jsonError.errorString());
    }
    DBusProxyConfig config;
    if (!doc.isNull()) {
        QJsonObject jsonObject = doc.object();
        config.enable = jsonObject.value("enable").toBool();
        config.busType = jsonObject.value("busType").toString();
        config.appId  = jsonObject.value("appId").toString();
        config.proxyPath = jsonObject.value("proxyPath").toString();
        config.name = QStringList { jsonObject.value("name").toString() };
        config.path = QStringList { jsonObject.value("path").toString() };
        config.interface = QStringList { jsonObject.value("interface").toString() };
    }
    return config;
}

int Cli::run()
{
    const auto appId = QString::fromStdString(m_args["APP"].asString());
    if (appId.isEmpty()) {
        p->printError(-1, "app id is empty");
        return -1;
    }

    linglong::service::RunParamOption paramOption;

    // FIXME(black_desk): It seems that paramOption should directly take a ref.
    linglong::package::Ref ref(appId);
    paramOption.appId = ref.appId;
    paramOption.version = ref.version;

    auto command = m_args["COMMAND"].asStringList();
    for (const auto &arg : command) {
        paramOption.exec.push_back(QString::fromStdString(arg));
    }

    // 获取用户环境变量
    QStringList envList = COMMAND_HELPER->getUserEnv(linglong::util::envList);
    if (!envList.isEmpty()) {
        paramOption.appEnv = envList;
    }

    // 判断是否设置了no-proxy参数
    paramOption.noDbusProxy = m_args["--no-dbus-proxy"].asBool();

    auto dbusProxyCfg = m_args["--dbus-proxy-cfg"].asString();
    if (!dbusProxyCfg.empty()) {
        auto data = parseDataFromProxyCfgFile(QString::fromStdString(dbusProxyCfg));
        if (!data.has_value()){
            p->printError(-1, "get empty data from cfg file");
            return -1;
        }
        // TODO(linxin): parse dbus filter info from config path
        paramOption.busType = "session";
        paramOption.filterName = data->name[0];
        paramOption.filterPath = data->path[0];
        paramOption.filterInterface = data->interface[0];
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
        p->printError(reply.code, reply.message);
        return -1;
    }
    return 0;
}

int Cli::exec() 
{
    const auto containerId = QString::fromStdString(m_args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
        return -1;
    }

    linglong::service::ExecParamOption execOption;
    execOption.containerID = containerId;

    auto &command = m_args["COMMAND"].asStringList();
    for (const auto &arg : command) {
        execOption.cmd.push_back(QString::fromStdString(arg));
    }

    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());
    const auto dbusReply = appManager.Exec(execOption);
    const auto reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kSuccess)) {
        p->printError(reply.code, reply.message);
        return -1;
    }
    return 0;
}

int Cli::enter()
{
    const auto containerId = QString::fromStdString(m_args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
        return -1;
    }

    const auto cmd = QString::fromStdString(m_args["COMMAND"].asString());
    if (cmd.isEmpty()) {
        p->printError(-1, "failed to get command list");
        return -1;
    }

    const auto pid = containerId.toInt();
    
    int reply = COMMAND_HELPER->namespaceEnter(pid, QStringList{ cmd });
    if (reply == -1) {
        p->printError(-1, "failed to enter namespace");
        return -1;
    }
    return 0;
}

int  Cli::ps()
{
    linglong::api::v1::dbus::AppManager1 appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());
    const auto outputFormat = m_args["--json"].asBool() ? "json" : "nojson";
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

int Cli::kill()
{
    const auto containerId = QString::fromStdString(m_args["PAGODA"].asString());
    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
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
        p->printError(reply.code, reply.message);
        return -1;
    }

    qInfo().noquote() << reply.message;
    return 0;
}

int Cli::install()
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
    const auto appId = QString::fromStdString(m_args["APP"].asString());
    linglong::package::Ref ref(appId);
    // 增加 channel/module
    installParamOption.channel = ref.channel;
    installParamOption.appModule = ref.module;
    installParamOption.appId = ref.appId;
    installParamOption.version = ref.version;
    installParamOption.arch = ref.arch;

    linglong::service::Reply reply;
    qInfo().noquote() << "install" << ref.appId << ", please wait a few minutes...";
    if (!m_args["--no-dbus"].asBool()) {
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
            p->printError(reply.code, reply.message);
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

int Cli::upgrade()
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());
    linglong::service::ParamOption paramOption;
    const auto appId = QString::fromStdString(m_args["APP"].asString());
    if (appId.isEmpty()) {
        p->printError(-1, "failed to get app id");
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
        p->printError(reply.code, reply.message);
        return -1;
    }
    qInfo().noquote() << "message:" << reply.message;
    return 0;
}

int Cli::search()
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    linglong::service::QueryParamOption paramOption;
    const auto appId = QString::fromStdString(m_args["APP"].asString());
    if (appId.isEmpty()) {
        p->printError(-1, "faied to get app id");
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
        p->printError(reply.code, reply.message);
        return -1;
    }

    auto [appMetaInfoList, err] =
      linglong::util::fromJSON<QList<QSharedPointer<linglong::package::AppMetaInfo>>>(
        reply.result.toLocal8Bit());
    if (err) {
        p->printError(-1, "failed to parse json reply");
        return -1;
    }

    // printAppInfo(appMetaInfoList);
    p->printResult(appMetaInfoList);
    return -1;
}

int Cli::uninstall()
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
    const QString appId = QString::fromStdString(m_args["APP"].asString());
    linglong::package::Ref ref(appId);
    paramOption.version = ref.version;
    paramOption.appId = appId;
    paramOption.channel = ref.channel;
    paramOption.appModule = ref.module;
    paramOption.delAppData = m_args["--prune"].asBool();
    linglong::service::Reply reply;
    qInfo().noquote() << "uninstall" << paramOption.appId << ", please wait a few minutes...";
    paramOption.delAllVersion = m_args["--all"].asBool();
    if (m_args["--no-dbus"].asBool()) {
        linglong::service::PackageManager packageManager;
        packageManager.setNoDBusMode(true);
        reply = packageManager.Uninstall(paramOption);
        if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
            p->printError(reply.code, reply.message);
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
        p->printError(reply.code, reply.message);
        return -1;
    }
    qInfo().noquote() << "message:" << reply.message;
    return 0;
}

int Cli::list() 
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    linglong::service::QueryParamOption paramOption;
    linglong::package::Ref ref(QString::fromStdString(m_args["APP"].asString()));
    paramOption.appId = ref.appId;
    QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList;
    linglong::service::QueryReply reply;
    if (m_args["--no-dbus"].asBool()) {
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
    // printAppInfo(appMetaInfoList);
    p->printResult(appMetaInfoList);
    return 0;
}

int  Cli::repo()
{
    OrgDeepinLinglongPackageManager1Interface sysPackageManager(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    if (m_args["modify"].asBool()) {
        const auto name = QString::fromStdString(m_args["--name"].asString());
        const auto url = QString::fromStdString(m_args["URL"].asString());
        linglong::service::Reply reply;
        if (!m_args["-no-dbus-proxy"].asBool()) {
            QDBusPendingReply<linglong::service::Reply> dbusReply =
              sysPackageManager.ModifyRepo(name, url);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
        } else {
            linglong::service::PackageManager packageManager;
            reply = packageManager.ModifyRepo(name, url);
        }
        if (reply.code != STATUS_CODE(kErrorModifyRepoSuccess)) {
            p->printError(reply.code, reply.message);
            return -1;
        }
        qInfo().noquote() << reply.message;
        return 0;
    }

    linglong::service::QueryReply reply;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply =
    sysPackageManager.getRepoInfo();
    dbusReply.waitForFinished();
    reply = dbusReply.value();

    qInfo().noquote() << QString("%1%2").arg("Name", -10).arg("Url");
    qInfo().noquote() << QString("%1%2").arg(reply.message, -10).arg(reply.result);
    return 0;
}