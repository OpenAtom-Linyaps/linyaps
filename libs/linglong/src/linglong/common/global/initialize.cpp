/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "initialize.h"

#include "configure.h"
#include "linglong/common/strings.h"

#include <qobjectdefs.h>
#include <systemd/sd-journal.h>

#include <QCoreApplication>

#include <csignal>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::common::global {

using namespace linglong::common;
using linglong::utils::log::LogBackend;
using linglong::utils::log::LogLevel;

namespace {
void catchUnixSignals(std::initializer_list<int> quitSignals)
{
    auto handler = [](int sig) -> void {
        LogI("Quit the application by signal({}).", sig);
        QCoreApplication::quit();
        GlobalTaskControl::cancel();
    };

    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    for (auto sig : quitSignals)
        sigaddset(&blocking_mask, sig);

    struct sigaction sa{};

    sa.sa_handler = handler;
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals)
        sigaction(sig, &sa, nullptr);
}

LogLevel parseLogLevel(const char *level)
{
    if (common::strings::stringEqual(level, "debug")) {
        return LogLevel::Debug;
    }

    if (common::strings::stringEqual(level, "info")) {
        return LogLevel::Info;
    }

    if (common::strings::stringEqual(level, "warning")) {
        return LogLevel::Warning;
    }

    if (common::strings::stringEqual(level, "error")) {
        return LogLevel::Error;
    }

    if (common::strings::stringEqual(level, "fatal")) {
        return LogLevel::Fatal;
    }

    return LogLevel::Info;
}

LogBackend parseLogBackend(const char *backends)
{
    LogBackend logBackend = LogBackend::None;

    const std::vector<std::string> backendsList = common::strings::split(backends, ',');
    for (const auto &backend : backendsList) {
        if (common::strings::stringEqual(backend, "console")) {
            logBackend = logBackend | LogBackend::Console;
        } else if (common::strings::stringEqual(backend, "journal")) {
            logBackend = logBackend | LogBackend::Journal;
        }
    }

    return logBackend;
}

} // namespace

void initLinyapsLogSystem(linglong::utils::log::LogBackend backend)
{
    LogLevel logLevel = LogLevel::Info;
    LogBackend logBackend = LogBackend::None;

    const char *logLevelEnv = getenv("LINYAPS_LOG_LEVEL");
    if (logLevelEnv) {
        logLevel = parseLogLevel(logLevelEnv);
    }

    const char *logBackendEnv = getenv("LINYAPS_LOG_BACKEND");
    if (logBackendEnv) {
        logBackend = parseLogBackend(logBackendEnv);
    } else {
        logBackend = backend;

        if (isatty(STDERR_FILENO)) {
            logBackend = logBackend | LogBackend::Console;
        }
    }

    setLogLevel(logLevel);
    setLogBackend(logBackend);
}

void applicationInitialize()
{
    catchUnixSignals({ SIGTERM, SIGQUIT, SIGINT, SIGHUP });
}

// Return current binary file is installed on the system
bool linglongInstalled()
{
    return QCoreApplication::applicationDirPath() == BINDIR;
}

auto globalTaskControl = GlobalTaskControl{};

const GlobalTaskControl *GlobalTaskControl::instance()
{
    return &globalTaskControl;
}

void GlobalTaskControl::cancel()
{
    globalTaskControl.cancelFlag.store(true);
    Q_EMIT globalTaskControl.OnCancel();
}

bool GlobalTaskControl::canceled()
{
    return globalTaskControl.cancelFlag.load();
}

} // namespace linglong::common::global
