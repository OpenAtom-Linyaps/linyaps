/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_CLI_H
#define LINGLONG_SRC_CLI_CLI_H

#include "linglong/api/v1/dbus/app_manager1.h"
#include "linglong/api/v1/dbus/package_manager1.h"
#include "linglong/package/package.h"
#include "linglong/package/ref.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/printer/printer.h"
#include "linglong/runtime/dbus_proxy.h"
#include "linglong/service/app_manager.h"
#include "linglong/util/app_status.h"
#include "linglong/util/command_helper.h"
#include "linglong/util/env.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/factory.h"
#include "linglong/utils/global/initialize.h"

#include <docopt.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>

namespace linglong {
namespace cli {

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

// add namespace
class Cli
{
public:
    Cli(DocOptMap &args,
        std::shared_ptr<Printer> printer,
        std::shared_ptr<Factory<linglong::api::v1::dbus::AppManager1>> appfn,
        std::shared_ptr<Factory<OrgDeepinLinglongPackageManager1Interface>> pkgfn,
        std::shared_ptr<Factory<linglong::util::CommandHelper>> cmdhelperfn,
        std::shared_ptr<Factory<linglong::service::PackageManager>> pkgmngfn);
    std::shared_ptr<Printer> p;

private:
    DocOptMap m_args;

    std::shared_ptr<Factory<linglong::api::v1::dbus::AppManager1>> appfn;
    std::shared_ptr<Factory<OrgDeepinLinglongPackageManager1Interface>> pkgfn;
    std::shared_ptr<Factory<linglong::util::CommandHelper>> cmdhelperfn;
    std::shared_ptr<Factory<linglong::service::PackageManager>> pkgmngfn;

public:
    int run();
    int exec();
    int enter();
    int ps();
    int kill();
    int install();
    int upgrade();
    int search();
    int uninstall();
    int list();
    int repo();
};

} // namespace cli
} // namespace linglong
#endif // LINGLONG_SRC_CLI_CLI_H