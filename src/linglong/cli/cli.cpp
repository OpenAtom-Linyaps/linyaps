#include "cli.h"

using namespace linglong::utils::error;
using namespace linglong::cli;

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
    if (data.size() == 0) {
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
        config.appId = jsonObject.value("appId").toString();
        config.proxyPath = jsonObject.value("proxyPath").toString();
        config.name = QStringList{ jsonObject.value("name").toString() };
        config.path = QStringList{ jsonObject.value("path").toString() };
        config.interface = QStringList{ jsonObject.value("interface").toString() };
    }
    return config;
}

Cli::Cli(std::map<std::string, docopt::value> &args,
         std::shared_ptr<Printer> printer,
         std::shared_ptr<Factory<linglong::api::v1::dbus::AppManager1>> appfn,
         std::shared_ptr<Factory<OrgDeepinLinglongPackageManager1Interface>> pkgfn,
         std::shared_ptr<Factory<linglong::util::CommandHelper>> cmdhelperfn,
         std::shared_ptr<Factory<linglong::service::PackageManager>> pkgmngfn)
    : m_args(args)
    , p(printer)
    , appfn(appfn)
    , pkgfn(pkgfn)
    , cmdhelperfn(cmdhelperfn)
    , pkgmngfn(pkgmngfn)
{
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
    if (!command.empty()) {
        for (const auto &arg : command) {
            paramOption.exec.push_back(QString::fromStdString(arg));
        }
    }

    // 获取用户环境变量
    QStringList envList = this->cmdhelperfn->builder()->getUserEnv(linglong::util::envList);
    if (!envList.isEmpty()) {
        paramOption.appEnv = envList;
    }
    // 判断是否设置了no-proxy参数
    paramOption.noDbusProxy = m_args["--no-dbus-proxy"].asBool();

    if (m_args["--dbus-proxy-cfg"].isString()) {
        auto dbusProxyCfg = QString::fromStdString(m_args["--dbus-proxy-cfg"].asString());
        if (!dbusProxyCfg.isEmpty()) {
            auto data = parseDataFromProxyCfgFile(dbusProxyCfg);
            if (!data.has_value()) {
                p->printError(-2, "get empty data from cfg file");
                return -1;
            }
            // TODO(linxin): parse dbus filter info from config path
            paramOption.busType = "session";
            paramOption.filterName = data->name[0];
            paramOption.filterPath = data->path[0];
            paramOption.filterInterface = data->interface[0];
        }
    }

    // TODO: ll-cli 进沙箱环境
    auto appManagerObj = appfn->builder();
    qDebug() << "send param" << paramOption.appId << "to service";
    auto dbusReply = appManagerObj->Start(paramOption);
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
    QString containerId;
    if (m_args["PAGODA"].isString()) {
        containerId = QString::fromStdString(m_args["PAGODA"].asString());
    }

    if (m_args["APP"].isString()) {
        containerId = QString::fromStdString(m_args["APP"].asString());
    }

    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
        return -1;
    }

    linglong::service::ExecParamOption execOption;
    execOption.containerID = containerId;

    if (m_args["COMMAND"].isStringList()) {
        auto &command = m_args["COMMAND"].asStringList();
        for (const auto &arg : command) {
            execOption.cmd.push_back(QString::fromStdString(arg));
        }
    }

    auto appManagerObj = this->appfn->builder();
    const auto dbusReply = appManagerObj->Exec(execOption);
    const auto reply = dbusReply.value();
    if (reply.code != STATUS_CODE(kSuccess)) {
        p->printError(reply.code, reply.message);
        return -1;
    }
    return 0;
}

int Cli::enter()
{
    QString containerId;
    if (m_args["PAGODA"].isString()) {
        containerId = QString::fromStdString(m_args["PAGODA"].asString());
    }

    if (m_args["APP"].isString()) {
        containerId = QString::fromStdString(m_args["APP"].asString());
    }

    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
        return -1;
    }

    QString cmd;
    if (m_args["COMMAND"].isString()) {
        cmd = QString::fromStdString(m_args["COMMAND"].asString());
        if (cmd.isEmpty()) {
            p->printError(-1, "failed to get command list");
            return -1;
        }
    }

    const auto pid = containerId.toInt();

    int reply = this->cmdhelperfn->builder()->namespaceEnter(pid, QStringList{ cmd });
    if (reply == -1) {
        p->printError(-1, "failed to enter namespace");
        return -1;
    }
    return 0;
}

int Cli::ps()
{
    auto appManagerObj = this->appfn->builder();
    const auto outputFormat = m_args["--json"].asBool() ? "json" : "nojson";
    const auto replyString = appManagerObj->ListContainer().value().result;

    QList<QSharedPointer<Container>> containerList;
    const auto doc = QJsonDocument::fromJson(replyString.toUtf8(), nullptr);
    if (doc.isArray()) {
        for (const auto container : doc.array()) {
            const auto str = QString(QJsonDocument(container.toObject()).toJson());
            QSharedPointer<Container> con(linglong::util::loadJsonString<Container>(str));
            containerList.push_back(con);
        }
    }
    this->cmdhelperfn->builder()->showContainer(containerList, outputFormat);
    return 0;
}

int Cli::kill()
{
    QString containerId;
    if (m_args["PAGODA"].isString()) {
        containerId = QString::fromStdString(m_args["PAGODA"].asString());
    }

    if (m_args["APP"].isString()) {
        containerId = QString::fromStdString(m_args["APP"].asString());
    }

    if (containerId.isEmpty()) {
        p->printError(-1, "failed to get container id");
        return -1;
    }
    // TODO: show kill result
    auto appManagerObj = this->appfn->builder();
    QDBusPendingReply<linglong::service::Reply> dbusReply = appManagerObj->Stop(containerId);
    dbusReply.waitForFinished();
    linglong::service::Reply reply = dbusReply.value();
    qDebug() << reply.message;
    if (reply.code != STATUS_CODE(kErrorPkgKillSuccess)) {
        p->printError(reply.code, reply.message);
        return -1;
    }

    qInfo().noquote() << reply.message;
    return 0;
}

int Cli::install()
{
    auto systemPkgObj = this->pkgfn->builder();
    // 收到中断信号后恢复操作
    signal(SIGINT, doIntOperate);
    // 设置 24 h超时
    systemPkgObj->setTimeout(1000 * 60 * 60 * 24);
    // appId format: org.deepin.calculator/1.2.6 in multi-version
    linglong::service::InstallParamOption installParamOption;
    auto tiers = m_args["TIER"].asStringList();
    if (tiers.empty()) {
        p->printError(-1, "failed to get app id");
        return -1;
    }
    for (auto &tier : tiers) {
        linglong::package::Ref ref(QString::fromStdString(tier));
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
              systemPkgObj->Install(installParamOption);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            qDebug() << reply.message;
            QThread::sleep(1);
            // 查询一次进度
            dbusReply = systemPkgObj->GetDownloadStatus(installParamOption, 0);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            bool disProgress = false;
            // 隐藏光标
            std::cout << "\033[?25l";
            while (reply.code == STATUS_CODE(kPkgInstalling)) {
                std::cout << "\r\33[K" << reply.message.toStdString();
                std::cout.flush();
                QThread::sleep(1);
                dbusReply = systemPkgObj->GetDownloadStatus(installParamOption, 0);
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
            auto pkgMngObj = this->pkgmngfn->builder();
            pkgMngObj->setNoDBusMode(true);
            reply = pkgMngObj->Install(installParamOption);
            pkgMngObj->pool->waitForDone(-1);
            qInfo().noquote() << "install " << installParamOption.appId << " done";
        }
    }
    return 0;
}

int Cli::upgrade()
{
    auto systemPkgObj = this->pkgfn->builder();
    linglong::service::ParamOption paramOption;
    auto tiers = m_args["TIER"].asStringList();

    if (tiers.empty()) {
        p->printError(-1, "failed to get app id");
        return -1;
    }

    for (auto &tier : tiers) {
        linglong::package::Ref ref(QString::fromStdString(tier));
        paramOption.arch = linglong::util::hostArch();
        paramOption.version = ref.version;
        paramOption.appId = ref.appId;
        // 增加 channel/module
        paramOption.channel = ref.channel;
        paramOption.appModule = ref.module;
        systemPkgObj->setTimeout(1000 * 60 * 60 * 24);
        qInfo().noquote() << "upgrade" << paramOption.appId << ", please wait a few minutes...";
        QDBusPendingReply<linglong::service::Reply> dbusReply = systemPkgObj->Update(paramOption);
        dbusReply.waitForFinished();
        linglong::service::Reply reply;
        reply = dbusReply.value();
        if (reply.code == STATUS_CODE(kPkgUpdating)) {
            signal(SIGINT, doIntOperate);
            QThread::sleep(1);
            dbusReply = systemPkgObj->GetDownloadStatus(paramOption, 1);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
            bool disProgress = false;
            // 隐藏光标
            std::cout << "\033[?25l";
            while (reply.code == STATUS_CODE(kPkgUpdating)) {
                std::cout << "\r\33[K" << reply.message.toStdString();
                std::cout.flush();
                QThread::sleep(1);
                dbusReply = systemPkgObj->GetDownloadStatus(paramOption, 1);
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
            p->printError(-1, reply.message);
            return -1;
        }
        qInfo().noquote() << "message:" << reply.message;
    }

    return 0;
}

int Cli::search()
{
    auto systemPkgObj = this->pkgfn->builder();
    linglong::service::QueryParamOption paramOption;
    QString appId;
    if (m_args["TEXT"].isString()) {
        appId = QString::fromStdString(m_args["TEXT"].asString());
    }
    if (appId.isEmpty()) {
        p->printError(-1, "faied to get app id");
        return -1;
    }
    paramOption.force = true;
    paramOption.appId = appId;
    systemPkgObj->setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = systemPkgObj->Query(paramOption);
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

    p->printResult(appMetaInfoList);
    return -1;
}

int Cli::uninstall()
{
    auto systemPkgObj = this->pkgfn->builder();

    systemPkgObj->setTimeout(1000 * 60 * 60 * 24);
    QDBusPendingReply<linglong::service::Reply> dbusReply;
    linglong::service::UninstallParamOption paramOption;
    // appId format: org.deepin.calculator/1.2.6 in multi-version
    // QStringList appInfoList = appInfo.split("/");

    auto tiers = m_args["TIER"].asStringList();

    for (auto &tier : tiers) {
        linglong::package::Ref ref(QString::fromStdString(tier));
        paramOption.version = ref.version;
        paramOption.appId = ref.appId;
        paramOption.channel = ref.channel;
        paramOption.appModule = ref.module;
        paramOption.delAppData = m_args["--prune"].asBool();
        linglong::service::Reply reply;
        qInfo().noquote() << "uninstall" << paramOption.appId << ", please wait a few minutes...";
        paramOption.delAllVersion = m_args["--all"].asBool();
        if (m_args["--no-dbus"].asBool()) {
            auto pkgMngObj = this->pkgmngfn->builder();
            pkgMngObj->setNoDBusMode(true);
            reply = pkgMngObj->Uninstall(paramOption);
            if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
                p->printError(reply.code, reply.message);
                return -1;
            } else {
                qInfo().noquote() << "uninstall " << paramOption.appId << " success";
            }
            return 0;
        }
        dbusReply = systemPkgObj->Uninstall(paramOption);
        dbusReply.waitForFinished();
        reply = dbusReply.value();

        if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
            p->printError(reply.code, reply.message);
            return -1;
        }
        qInfo().noquote() << "message:" << reply.message;
    }

    return 0;
}

int Cli::list()
{
    auto systemPkgObj = this->pkgfn->builder();

    linglong::service::QueryParamOption paramOption;
    QString appId;
    if (m_args["APP"].isString()) {
        appId = QString::fromStdString(m_args["APP"].asString());
    }
    linglong::package::Ref ref(appId);
    paramOption.appId = ref.appId;
    QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList;
    linglong::service::QueryReply reply;
    if (m_args["--no-dbus"].asBool()) {
        auto pkgMngObj = this->pkgmngfn->builder();
        reply = pkgMngObj->Query(paramOption);
    } else {
        QDBusPendingReply<linglong::service::QueryReply> dbusReply =
          systemPkgObj->Query(paramOption);
        // 默认超时时间为25s
        dbusReply.waitForFinished();
        reply = dbusReply.value();
    }
    linglong::util::getAppMetaInfoListByJson(reply.result, appMetaInfoList);
    // printAppInfo(appMetaInfoList);
    p->printResult(appMetaInfoList);
    return 0;
}

int Cli::repo()
{
    auto systemPkgObj = this->pkgfn->builder();

    if (m_args["modify"].asBool()) {
        QString name;
        QString url;
        if (m_args["--name"].isString()) {
            name = QString::fromStdString(m_args["--name"].asString());
        }
        if (m_args["URL"].isString()) {
            name = QString::fromStdString(m_args["URL"].asString());
        }
        linglong::service::Reply reply;
        if (!m_args["-no-dbus-proxy"].asBool()) {
            QDBusPendingReply<linglong::service::Reply> dbusReply =
              systemPkgObj->ModifyRepo(name, url);
            dbusReply.waitForFinished();
            reply = dbusReply.value();
        } else {
            auto pkgMngObj = this->pkgmngfn->builder();
            reply = pkgMngObj->ModifyRepo(name, url);
        }
        if (reply.code != STATUS_CODE(kErrorModifyRepoSuccess)) {
            p->printError(reply.code, reply.message);
            return -1;
        }
        qInfo().noquote() << reply.message;
        return 0;
    }

    linglong::service::QueryReply reply;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = systemPkgObj->getRepoInfo();
    dbusReply.waitForFinished();
    reply = dbusReply.value();

    qInfo().noquote() << QString("%1%2").arg("Name", -10).arg("Url");
    qInfo().noquote() << QString("%1%2").arg(reply.message, -10).arg(reply.result);
    return 0;
}