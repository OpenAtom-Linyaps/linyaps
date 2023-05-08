/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/app_manager.h"
#include "module/package/package.h"
#include "module/package_manager.h"
#include "module/util/app_status.h"
#include "module/util/command_helper.h"
#include "module/util/env.h"
#include "module/util/qserializer/json.h"
#include "module/util/status_code.h"
#include "module/util/sysinfo.h"
#include "module/util/xdg.h"
#include "package_manager/impl/package_manager.h"
#include "service/impl/app_manager.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>
#include <iostream>

static qint64 systemHelperPid = -1;

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

/**
 * @brief 检测ll-service dbus服务是否已经启动，未启动则启动
 *
 * @param packageManager ll-service dbus服务
 *
 */
static void checkAndStartService(OrgDeepinLinglongAppManagerInterface &appManager)
{
    const auto kStatusActive = "active";
    QDBusReply<QString> status = appManager.Status();
    // FIXME: should use more precision to check status
    if (kStatusActive != status.value()) {
        QProcess process;
        process.setProgram("ll-service");
        process.setStandardOutputFile("/dev/null");
        process.setStandardErrorFile("/dev/null");
        process.setArguments({});
        process.startDetached();

        for (int i = 0; i < 10; ++i) {
            status = appManager.Status();
            if (kStatusActive == status.value()) {
                return;
            }
            QThread::sleep(1);
        }

        qCritical() << "start ll-service failed";
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

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    QCommandLineParser parser;
    parser.addHelpOption();
    QStringList subCommandList = { "run",       "ps",     "exec",  "kill", "install",
                                   "uninstall", "update", "query", "list", "repo" };

    parser.addPositionalArgument("subcommand",
                                 subCommandList.join("\n"),
                                 "subcommand [sub-option]");
    auto optNoDbus = QCommandLineOption("nodbus",
                                        "execute cmd directly, not via dbus(only for root user)",
                                        "");
    optNoDbus.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({ optNoDbus });
    parser.parse(QCoreApplication::arguments());

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    OrgDeepinLinglongAppManagerInterface appManager("org.deepin.linglong.AppManager",
                                                    "/org/deepin/linglong/AppManager",
                                                    QDBusConnection::sessionBus());

    OrgDeepinLinglongPackageManagerInterface sysPackageManager(
            "org.deepin.linglong.PackageManager",
            "/org/deepin/linglong/PackageManager",
            QDBusConnection::systemBus());

    auto systemHelperDBusConnection = QDBusConnection::systemBus();
    const auto systemHelperAddress = QString("unix:path=/run/linglong_system_helper_socket");

    if (parser.isSet(optNoDbus)) {
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

    if (!parser.isSet(optNoDbus)) {
        checkAndStartService(appManager);
    }

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        { "run", // 启动玲珑应用
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("run", "run application", "run");
              parser.addPositionalArgument("appId", "application id", "com.deepin.demo");

              const auto optExec = QCommandLineOption("exec", "run exec", "/bin/bash");
              parser.addOption(optExec);

              const auto optNoProxy =
                      QCommandLineOption("no-proxy", "whether to use dbus proxy in box", "");

              const auto optNameFilter =
                      QCommandLineOption("filter-name",
                                         "dbus name filter to use",
                                         "--filter-name=org.deepin.linglong.AppManager",
                                         "");
              const auto optPathFilter =
                      QCommandLineOption("filter-path",
                                         "dbus path filter to use",
                                         "--filter-path=/org/deepin/linglong/AppManager",
                                         "");
              const auto optInterfaceFilter =
                      QCommandLineOption("filter-interface",
                                         "dbus interface filter to use",
                                         "--filter-interface=org.deepin.linglong.AppManager",
                                         "");

              // 增加channel/module
              const auto optChannel = QCommandLineOption("channel",
                                                         "the channel of app",
                                                         "--channel=linglong",
                                                         "linglong");
              parser.addOption(optChannel);
              const auto optModule = QCommandLineOption("module",
                                                        "the module of app",
                                                        "--module=runtime",
                                                        "runtime");
              parser.addOption(optModule);

              parser.addOption(optNoProxy);
              parser.addOption(optNameFilter);
              parser.addOption(optPathFilter);
              parser.addOption(optInterfaceFilter);
              parser.process(app);
              args = parser.positionalArguments();
              const auto appId = args.value(1);
              if (appId.isEmpty()) {
                  parser.showHelp();
                  return -1;
              }

              auto exec = parser.value(optExec);
              // 转化特殊字符
              args = linglong::util::convertSpecialCharacters(args);

              // 移除run appid两个参数 获取 exec 执行参数
              // eg: ll-cli run deepin-music --exec deepin-music /usr/share/music/test.mp3
              // exec = "deepin-music /usr/share/music/test.mp3"
              QString desktopArgs;
              desktopArgs.clear();
              if (args.length() > 2 && !exec.isEmpty()) {
                  args.removeAt(0);
                  args.removeAt(0);
                  desktopArgs = args.join(" ");
                  exec = QStringList{ exec, desktopArgs }.join(" ");
              }

              linglong::service::RunParamOption paramOption;
              // appId format: org.deepin.calculator/1.2.6 in multi-version
              QMap<QString, QString> paramMap;
              QStringList appInfoList = appId.split("/");
              paramOption.appId = appId;
              if (appInfoList.size() > 1) {
                  paramOption.appId = appInfoList.at(0);
                  paramOption.version = appInfoList.at(1);
              }
              if (!exec.isEmpty()) {
                  paramOption.exec = exec;
              }

              paramOption.channel = parser.value(optChannel);
              paramOption.appModule = parser.value(optModule);

              // 获取用户环境变量
              QStringList envList = COMMAND_HELPER->getUserEnv(linglong::util::envList);
              if (!envList.isEmpty()) {
                  paramOption.appEnv = envList.join(",");
              }

              // 判断是否设置了no-proxy参数
              paramOption.noDbusProxy = parser.isSet(optNoProxy);
              if (!parser.isSet(optNoProxy)) {
                  // FIX to do only deal with session bus
                  paramOption.busType = "session";
                  paramOption.filterName = parser.value(optNameFilter);
                  paramOption.filterPath = parser.value(optPathFilter);
                  paramOption.filterInterface = parser.value(optInterfaceFilter);
              }

              // ll-cli 进沙箱环境
              linglong::service::Reply reply;
              if ("/bin/bash" == parser.value(optExec) || "bash" == parser.value(optExec)) {
                  reply = APP_MANAGER->Start(paramOption);
                  APP_MANAGER->runPool->waitForDone(-1);
                  if (0 != reply.code) {
                      qCritical().noquote()
                              << "message:" << reply.message << ", errcode:" << reply.code;
                      return -1;
                  }
                  qDebug() << "exit with in sandbox";
                  return 0;
              }

              qDebug() << "send param" << paramOption.appId << "to service";
              QDBusPendingReply<linglong::service::Reply> dbusReply = appManager.Start(paramOption);
              dbusReply.waitForFinished();
              reply = dbusReply.value();
              if (reply.code != 0) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }
              return 0;
          } },
        { "exec", // exec new command in a existed container.
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("exec", "exec command in container", "exec");
              parser.addPositionalArgument("containerId",
                                           "container id",
                                           "aebbe2f455cf443f89d5c92f36d154dd");
              parser.addPositionalArgument("cmd", "command", "\"bash\"");
              const auto envArg = QCommandLineOption({ "e", "env" },
                                                     "extra environment variables splited by comma",
                                                     "env",
                                                     "");
              const auto pwdArg = QCommandLineOption({ "d", "path" },
                                                     "location to exec the new command",
                                                     "pwd",
                                                     qgetenv("HOME"));
              parser.addOption(envArg);
              parser.addOption(pwdArg);
              parser.process(app);

              const auto containerId = parser.positionalArguments().value(1);
              if (containerId.isEmpty()) {
                  parser.showHelp();
                  return -1;
              }

              const auto cmd = parser.positionalArguments().value(2);
              if (cmd.isEmpty()) {
                  parser.showHelp();
                  return -1;
              }

              linglong::service::ExecParamOption execOption;
              execOption.cmd = cmd;
              execOption.containerID = containerId;

              const auto envs = parser.value(envArg).split(",",
#ifdef QT_DEPRECATED_VERSION_5_15
                                                           Qt::SkipEmptyParts
#else
                                                           QString::SkipEmptyParts
#endif
              );
              QStringList envList = envs;
              execOption.env = envList.join(",");

              const auto dbusReply = appManager.Exec(execOption);
              const auto reply = dbusReply.value();
              if (reply.code != STATUS_CODE(kSuccess)) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }
              return 0;
          } },
        { "enter",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("containerId",
                                           "container id",
                                           "aebbe2f455cf443f89d5c92f36d154dd");
              parser.addPositionalArgument("exec", "exec command in container", "/bin/bash");
              parser.process(app);

              const auto containerId = parser.positionalArguments().value(1);
              if (containerId.isEmpty()) {
                  parser.showHelp();
                  return -1;
              }

              const auto cmd = parser.positionalArguments().value(2);
              if (cmd.isEmpty()) {
                  parser.showHelp();
                  return -1;
              }

              const auto pid = containerId.toInt();

              return COMMAND_HELPER->namespaceEnter(pid, QStringList{ cmd });
          } },
        { "ps", // 查看玲珑沙箱进程
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("ps", "show running applications", "ps");

              const auto optOutputFormat =
                      QCommandLineOption("output-format", "json/console", "console");
              parser.addOption(optOutputFormat);

              parser.process(app);

              const auto outputFormat = parser.value(optOutputFormat);
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
          } },
        { "kill", // 关闭玲珑沙箱
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("kill", "kill container with id", "kill");
              parser.addPositionalArgument("container-id", "container id", "");

              parser.process(app);
              args = parser.positionalArguments();

              const auto containerId = args.value(1).trimmed();
              if (containerId.isEmpty() || args.size() > 2) {
                  parser.showHelp();
                  return -1;
              }
              // TODO: show kill result
              QDBusPendingReply<linglong::service::Reply> dbusReply = appManager.Stop(containerId);
              dbusReply.waitForFinished();
              linglong::service::Reply reply = dbusReply.value();
              if (reply.code != STATUS_CODE(kErrorPkgKillSuccess)) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }

              qInfo().noquote() << reply.message;
              return 0;
          } },
        { "install", // 下载玲珑包
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("install", "install an application", "install");
              parser.addPositionalArgument("appId", "application id", "com.deepin.demo");

              const auto optChannel = QCommandLineOption("channel",
                                                         "the channel of app",
                                                         "--channel=linglong",
                                                         "linglong");
              parser.addOption(optChannel);
              const auto optModule = QCommandLineOption("module",
                                                        "the module of app",
                                                        "--module=runtime",
                                                        "runtime");
              parser.addOption(optModule);

              parser.process(app);

              args = parser.positionalArguments();
              // 参数个数校验
              if (args.size() != 2) {
                  parser.showHelp(-1);
                  return -1;
              }

              // 收到中断信号后恢复操作
              signal(SIGINT, doIntOperate);
              // 设置 24 h超时
              sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
              // appId format: org.deepin.calculator/1.2.6 in multi-version
              linglong::service::InstallParamOption installParamOption;
              // 增加 channel/module
              installParamOption.channel = parser.value(optChannel);
              installParamOption.appModule = parser.value(optModule);
              QStringList appInfoList = args.at(1).split("/");
              installParamOption.appId = appInfoList.at(0);
              installParamOption.arch = linglong::util::hostArch();
              if (appInfoList.size() == 2) {
                  installParamOption.version = appInfoList.at(1);
              } else if (appInfoList.size() == 3) {
                  installParamOption.version = appInfoList.at(1);
                  installParamOption.arch = appInfoList.at(2);
              }

              linglong::service::Reply reply;
              qInfo().noquote() << "install" << args.at(1) << ", please wait a few minutes...";
              if (!parser.isSet(optNoDbus)) {
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
                      qCritical().noquote()
                              << "message:" << reply.message << ", errcode:" << reply.code;
                      return -1;
                  } else {
                      qInfo().noquote() << "message:" << reply.message;
                  }
              } else {
                  PACKAGE_MANAGER->setNoDBusMode(true);
                  reply = PACKAGE_MANAGER->Install(installParamOption);
                  PACKAGE_MANAGER->pool->waitForDone(-1);
                  qInfo().noquote() << "install " << installParamOption.appId << " done";
              }
              return 0;
          } },
        { "update", // 更新玲珑包
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("update", "update an application", "update");
              parser.addPositionalArgument("appId", "application id", "com.deepin.demo");

              const auto optChannel = QCommandLineOption("channel",
                                                         "the channel of app",
                                                         "--channel=linglong",
                                                         "linglong");
              parser.addOption(optChannel);
              const auto optModule = QCommandLineOption("module",
                                                        "the module of app",
                                                        "--module=runtime",
                                                        "runtime");
              parser.addOption(optModule);

              parser.process(app);
              linglong::service::ParamOption paramOption;
              args = parser.positionalArguments();
              QStringList appInfoList = args.value(1).split("/");
              if (args.size() != 2) {
                  parser.showHelp(-1);
                  return -1;
              }
              paramOption.appId = appInfoList.at(0).trimmed();
              if (paramOption.appId.isEmpty()) {
                  parser.showHelp(-1);
                  return -1;
              }
              paramOption.arch = linglong::util::hostArch();
              if (appInfoList.size() > 1) {
                  paramOption.version = appInfoList.at(1);
              }
              // 增加 channel/module
              paramOption.channel = parser.value(optChannel);
              paramOption.appModule = parser.value(optModule);
              sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
              qInfo().noquote() << "update" << paramOption.appId
                                << ", please wait a few minutes...";
              QDBusPendingReply<linglong::service::Reply> dbusReply =
                      sysPackageManager.Update(paramOption);
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
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }
              qInfo().noquote() << "message:" << reply.message;
              return 0;
          } },
        { "query", // 查询玲珑包
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("query", "query app info", "query");
              parser.addPositionalArgument("appId", "application id", "com.deepin.demo");
              const auto optNoCache =
                      QCommandLineOption("force", "query from server directly, not from cache", "");
              parser.addOption(optNoCache);
              parser.process(app);

              args = parser.positionalArguments();
              if (args.size() != 2) {
                  parser.showHelp(-1);
                  return -1;
              }

              linglong::service::QueryParamOption paramOption;
              paramOption.appId = args.value(1).trimmed();
              if (paramOption.appId.isEmpty()) {
                  parser.showHelp(-1);
                  return -1;
              }
              paramOption.force = parser.isSet(optNoCache);
              paramOption.appId = args.value(1);
              sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
              QDBusPendingReply<linglong::service::QueryReply> dbusReply =
                      sysPackageManager.Query(paramOption);
              dbusReply.waitForFinished();
              linglong::service::QueryReply reply = dbusReply.value();
              if (reply.code != STATUS_CODE(kErrorPkgQuerySuccess)) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }

              auto [appMetaInfoList, err] = linglong::util::fromJSON<
                      QList<QSharedPointer<linglong::package::AppMetaInfo>>>(
                      reply.result.toLocal8Bit());
              if (err) {
                  qCritical() << "parse json reply failed:" << err;
                  return -1;
              }

              printAppInfo(appMetaInfoList);
              return 0;
          } },
        { "uninstall", // 卸载玲珑包
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("uninstall", "uninstall an application", "uninstall");
              parser.addPositionalArgument("appId", "application id", "com.deepin.demo");
              const auto optAllVer =
                      QCommandLineOption("all-version", "uninstall all version application", "");
              const auto optDelData = QCommandLineOption("delete-data", "delete app data", "");
              parser.addOption(optAllVer);
              parser.addOption(optDelData);

              const auto optChannel = QCommandLineOption("channel",
                                                         "the channel of app",
                                                         "--channel=linglong",
                                                         "linglong");
              parser.addOption(optChannel);
              const auto optModule = QCommandLineOption("module",
                                                        "the module of app",
                                                        "--module=runtime",
                                                        "runtime");
              parser.addOption(optModule);

              parser.process(app);

              args = parser.positionalArguments();
              if (args.size() != 2) {
                  parser.showHelp(-1);
                  return -1;
              }
              const auto appInfo = args.value(1);
              sysPackageManager.setTimeout(1000 * 60 * 60 * 24);
              QDBusPendingReply<linglong::service::Reply> dbusReply;
              linglong::service::UninstallParamOption paramOption;
              // appId format: org.deepin.calculator/1.2.6 in multi-version
              QStringList appInfoList = appInfo.split("/");
              paramOption.appId = appInfo;
              if (appInfoList.size() > 1) {
                  paramOption.appId = appInfoList.at(0);
                  paramOption.version = appInfoList.at(1);
              }

              paramOption.channel = parser.value(optChannel);
              paramOption.appModule = parser.value(optModule);
              paramOption.delAppData = parser.isSet(optDelData);
              linglong::service::Reply reply;
              qInfo().noquote() << "uninstall" << appInfo << ", please wait a few minutes...";
              paramOption.delAllVersion = parser.isSet(optAllVer);
              if (parser.isSet(optNoDbus)) {
                  PACKAGE_MANAGER->setNoDBusMode(true);
                  reply = PACKAGE_MANAGER->Uninstall(paramOption);
                  if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
                      qInfo().noquote()
                              << "message: " << reply.message << ", errcode:" << reply.code;
                      return -1;
                  } else {
                      qInfo().noquote() << "uninstall " << appInfo << " success";
                  }
                  return 0;
              }
              dbusReply = sysPackageManager.Uninstall(paramOption);
              dbusReply.waitForFinished();
              reply = dbusReply.value();

              if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }
              qInfo().noquote() << "message:" << reply.message;
              return 0;
          } },
        { "list", // 查询已安装玲珑包
          [&](QCommandLineParser &parser) -> int {
              const auto optType = QCommandLineOption("type",
                                                      "query installed app",
                                                      "--type=installed",
                                                      "installed");
              parser.clearPositionalArguments();
              parser.addPositionalArgument("list", "show installed application", "list");
              parser.addOption(optType);
              parser.process(app);
              const auto optPara = parser.value(optType);
              if ("installed" != optPara) {
                  parser.showHelp(-1);
                  return -1;
              }

              args = parser.positionalArguments();
              if (args.size() != 1) {
                  parser.showHelp(-1);
                  return -1;
              }

              linglong::service::QueryParamOption paramOption;
              paramOption.appId = optPara;
              QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList;
              linglong::service::QueryReply reply;
              if (parser.isSet(optNoDbus)) {
                  reply = PACKAGE_MANAGER->Query(paramOption);
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
          } },
        { "repo",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("repo", "config remote repo", "repo");
              parser.addPositionalArgument("modify", "modify the url of repo", "modify");
              parser.addPositionalArgument("url", "the url of repo", "");
              const auto optRepoName =
                      QCommandLineOption("name", "the name of remote repo", "repo name", "repo");
              parser.addOption(optRepoName);
              parser.process(app);
              args = parser.positionalArguments();
              const auto subCmd = args.value(1).trimmed();
              const auto url = args.value(2).trimmed();
              const auto name = parser.value(optRepoName);
              if (url.isEmpty() || "modify" != subCmd || args.size() != 3) {
                  parser.showHelp(-1);
                  return -1;
              }
              linglong::service::Reply reply;
              if (!parser.isSet(optNoDbus)) {
                  QDBusPendingReply<linglong::service::Reply> dbusReply =
                          sysPackageManager.ModifyRepo(name, url);
                  dbusReply.waitForFinished();
                  reply = dbusReply.value();
              } else {
                  reply = PACKAGE_MANAGER->ModifyRepo(name, url);
              }
              if (reply.code != STATUS_CODE(kErrorModifyRepoSuccess)) {
                  qCritical().noquote()
                          << "message:" << reply.message << ", errcode:" << reply.code;
                  return -1;
              }
              qInfo().noquote() << reply.message;
              return 0;
          } },
    };

    if (subcommandMap.contains(command)) {
        const auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
