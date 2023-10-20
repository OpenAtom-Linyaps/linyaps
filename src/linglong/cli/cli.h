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
#include "linglong/service/app_manager.h"
#include "linglong/util/app_status.h"
#include "linglong/util/command_helper.h"
#include "linglong/util/env.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/package/ref.h"
#include "linglong/runtime/dbus_proxy.h"
#include "linglong/utils/error/error.h"
#include "linglong/printer/printer.h"

#include <docopt.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>
#include <cstddef>
#include <fstream>
#include <memory>

class Cli {
public:
    Cli(std::map<std::string, docopt::value> &args, std::unique_ptr<Printer> printer );

private:
    std::unique_ptr<Printer> p;

    std::map<std::string, docopt::value> m_args;

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

#endif // LINGLONG_SRC_CLI_CLI_H