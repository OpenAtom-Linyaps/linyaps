/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"
#include "linglong/printer/printer.h"
#include "linglong/utils/error/error.h"

#include <qt5/QtCore/qjsondocument.h>
#include <qt5/QtCore/qjsonobject.h>

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>

using namespace linglong::utils::error;
using namespace linglong::package;

static qint64 systemHelperPid = -1;

typedef std::map<std::string, docopt::value> DocOptMap;

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

// /**
//  * @brief 统计字符串中中文字符的个数
//  *
//  * @param name 软件包名称
//  * @return int 中文字符个数
//  */
// static int getUnicodeNum(const QString &name)
// {
//     int num = 0;
//     int count = name.count();
//     for (int i = 0; i < count; i++) {
//         QChar ch = name.at(i);
//         ushort decode = ch.unicode();
//         if (decode >= 0x4E00 && decode <= 0x9FA5) {
//             num++;
//         }
//     }
//     return num;
// }



static void handleOnExit(int, void *)
{
    if (systemHelperPid != -1) {
        kill(systemHelperPid, SIGTERM);
    }
}

// /**
//  * @brief 输出软件包的查询结果
//  *
//  * @param appMetaInfoList 软件包元信息列表
//  *
//  */
// static void printAppInfo(QList<QSharedPointer<linglong::package::AppMetaInfo>> appMetaInfoList)
// {
//     if (appMetaInfoList.size() > 0) {
//         qInfo("\033[1m\033[38;5;214m%-32s%-32s%-16s%-12s%-16s%-12s%-s\033[0m",
//               qUtf8Printable("appId"),
//               qUtf8Printable("name"),
//               qUtf8Printable("version"),
//               qUtf8Printable("arch"),
//               qUtf8Printable("channel"),
//               qUtf8Printable("module"),
//               qUtf8Printable("description"));
//         for (const auto &it : appMetaInfoList) {
//             QString simpleDescription = it->description.trimmed();
//             if (simpleDescription.length() > 56) {
//                 simpleDescription = it->description.trimmed().left(53) + "...";
//             }
//             QString appId = it->appId.trimmed();
//             QString name = it->name.trimmed();
//             if (name.length() > 32) {
//                 name = it->name.trimmed().left(29) + "...";
//             }
//             if (appId.length() > 32) {
//                 name.push_front(" ");
//             }
//             int count = getUnicodeNum(name);
//             int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
//             qInfo().noquote() << QString("%1%2%3%4%5%6%7")
//                                    .arg(appId, -32, QLatin1Char(' '))
//                                    .arg(name, count - 32, QLatin1Char(' '))
//                                    .arg(it->version.trimmed(), -16, QLatin1Char(' '))
//                                    .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
//                                    .arg(it->channel.trimmed(), -16, QLatin1Char(' '))
//                                    .arg(it->module.trimmed(), -12, QLatin1Char(' '))
//                                    .arg(simpleDescription, -length, QLatin1Char(' '));
//         }
//     } else {
//         qInfo().noquote() << "app not found in repo";
//     }
// }

static void startDaemon(QString program, QStringList args = {}, qint64 *pid = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setStandardOutputFile("/dev/null");
    process.setStandardErrorFile("/dev/null");
    process.setArguments(args);
    process.startDetached(pid);
}

// static int execCommandSearchAndList(std::function<Result<QList<QSharedPointer<AppMetaInfo>>>()> cmd, DocOptMap &args)
// {
//     bool json = args["--json"].asBool();
//     Result<QList<QSharedPointer<AppMetaInfo>>> result = cmd();
//     if (!result.has_value()){
//         return -1;
//     }
//     return 0;
// }

// static int execCommand(std::function<Result<int>()> cmd, DocOptMap &args)
// {
//     Result<int> result = cmd();
//     if (!result.has_value()){
//         return -1;
//     }
//     return 0;
// }

// // 输入 处理 输出
// // 文本打印内容，
// static int subcommandRun(DocOptMap &args)
// {
//     std::unique_ptr<Printer> p = std::make_unique<Printer>(args["--json"].asBool());
//     Cli cli = Cli(args, p);
//     Result<int> result = cli.run();
//     if (!result.has_value()){
//         return -1;
//     }
//     return 0; 
// }

// // Exec new command in an existing pagoda.
// static int subcommandExec(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.exec, args);
// }

// // Enter an existing pagoda, run an interactive bash shell in that pagoda.
// static int subcommandEnter(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.enter, args);
// }

// // List all existing pagodas.
// static int subcommandPs(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.ps, args);
// }

// // Stop a pagoda.
// static int subcommandKill(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.kill, args);
// }

// // Install package.
// static int subcommandInstall(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.install, args);
// }

// // Upgrade package, which means install the new version and remove old version of that package.
// static int subcommandUpgrade(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.upgrade, args);
// }

// // Search tiers online.
// static int subcommandSearch(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommandSearchAndList(cli.search, args);
// }

// // Uninstall a tier.
// static int subcommandUninstall(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.uninstall, args);
// }

// // List all installed tiers.
// static int subcommandList(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommandSearchAndList(cli.list, args);
// }

// // Modify linglong tier repo settings.
// static int subcommandRepo(DocOptMap &args)
// {
//     Cli cli = Cli(args);
//     return execCommand(cli.repo, args);
// }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    DocOptMap args =
      docopt::docopt(USAGE,
                     { argv + 1, argv + argc },
                     true,                  // show help if requested
                     "linglong CLI 1.4.0"); // version string

    auto systemHelperDBusConnection = QDBusConnection::systemBus();
    const auto systemHelperAddress = QString("unix:path=/run/linglong_system_helper_socket");

    if (args["--no-dbus"].asBool()) {
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

    std::unique_ptr<Printer> p;
    if (args["--json"].asBool())
    {
        p = std::make_unique<PrinterJson>();
    } else {
        p = std::make_unique<PrinterNormal>();
    }

    Cli cli = Cli(args, std::move(p));
    
    QMap<QString, std::function<int()>> subcommandMap = {
        { "run", [&cli](){ return cli.run();} },
        { "exec", [&cli](){ return cli.exec();} },
        { "enter", [&cli](){ return cli.enter();} },
        { "ps", [&cli](){ return cli.ps();} },
        { "kill", [&cli](){ return cli.kill();} },
        { "install", [&cli](){ return cli.install();} },
        { "upgrade", [&cli](){ return cli.upgrade();} },
        { "search", [&cli](){ return cli.search();} },
        { "uninstall", [&cli](){ return cli.uninstall();} },
        { "list", [&cli](){ return cli.list();} },
        { "repo", [&cli](){ return cli.repo();} },
    };

    for (const auto &subcommand : subcommandMap.keys()) {
        if (args[subcommand.toStdString()].asBool() == true) {
            return subcommandMap[subcommand]();
        }
    }
}
