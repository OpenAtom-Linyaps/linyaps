/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/global/initialize.h"

#include "configure.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/string.h"

#include <qcoreapplication.h>
#include <qloggingcategory.h>
#include <qobjectdefs.h>
#include <systemd/sd-journal.h>

#include <QCoreApplication>
#include <QDebug>

#include <csignal>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::utils::global {

using linglong::utils::log::LogBackend;
using linglong::utils::log::LogLevel;

namespace {
void catchUnixSignals(std::initializer_list<int> quitSignals)
{
    auto handler = [](int sig) -> void {
        qInfo().noquote() << QString("Quit the application by signal(%1).").arg(sig);
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

bool forceStderrLogging = false;

auto shouldLogToStderr() -> bool
{
    return forceStderrLogging || isatty(STDERR_FILENO);
}

void linglong_message_handler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &message)
{
    QString formattedMessage = qFormatLogMessage(type, context, message);
    // 非tty环境可能是从systemd启动的应用，为避免和下面的sd_journal输出重复，不输出日志到标准错误流
    if (shouldLogToStderr()) {
        // print nothing if message pattern didn't apply / was empty.
        // (still print empty lines, e.g. because message itself was empty)
        if (formattedMessage.isNull())
            return;

        fprintf(stderr,
                "(%d) %s:%d %s\n",
                getpid(),
                context.file,
                context.line,
                formattedMessage.toLocal8Bit().constData());
        fflush(stderr);
    }

    int priority = LOG_INFO; // Informational
    switch (type) {
    case QtDebugMsg:
        priority = LOG_DEBUG; // Debug-level messages
        break;
    case QtInfoMsg:
        priority = LOG_INFO; // Informational conditions
        break;
    case QtWarningMsg:
        priority = LOG_WARNING; // Warning conditions
        break;
    case QtCriticalMsg:
        priority = LOG_CRIT; // Critical conditions
        break;
    case QtFatalMsg:
        priority = LOG_ALERT; // Action must be taken immediately
        break;
    }

    auto file = QString("CODE_FILE=%1").arg(context.file ? context.file : "unknown");
    auto line = QString("CODE_LINE=%1").arg(context.line);

    sd_journal_send_with_location(file.toUtf8().constData(),
                                  line.toUtf8().constData(),
                                  context.function ? context.function : "unknown",
                                  "MESSAGE=%s",
                                  formattedMessage.toUtf8().constData(),
                                  "PRIORITY=%i",
                                  priority,
                                  "QT_CATEGORY=%s",
                                  context.category ? context.category : "unknown",
                                  NULL);

    return; // Prevent further output to stderr
}

LogLevel parseLogLevel(const char *level)
{
    if (stringEqual(level, "debug")) {
        return LogLevel::Debug;
    } else if (stringEqual(level, "info")) {
        return LogLevel::Info;
    } else if (stringEqual(level, "warning")) {
        return LogLevel::Warning;
    } else if (stringEqual(level, "error")) {
        return LogLevel::Error;
    } else if (stringEqual(level, "fatal")) {
        return LogLevel::Fatal;
    }

    return LogLevel::Info;
}

LogBackend parseLogBackend(const char *backends)
{
    LogBackend logBackend = LogBackend::None;

    std::vector<std::string> backendsList = splitString(backends, ',');
    for (auto backend : backendsList) {
        if (stringEqual(backend, "console")) {
            logBackend = logBackend | LogBackend::Console;
        } else if (stringEqual(backend, "journal")) {
            logBackend = logBackend | LogBackend::Journal;
        }
    }

    return logBackend;
}

} // namespace

void initLinyapsLogSystem(const char *command)
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
        if (command == std::string("ll-builder")) {
            logBackend = LogBackend::Console;
        } else if (command == std::string("ll-cli")) {
            logBackend = LogBackend::Journal;
        } else if (command == std::string("ll-package-manager")) {
            logBackend = LogBackend::Journal;
        }

        if (isatty(STDERR_FILENO)) {
            logBackend = logBackend | LogBackend::Console;
        }
    }

    setLogLevel(logLevel);
    setLogBackend(logBackend);
}

void applicationInitialize(bool appForceStderrLogging)
{
    QCoreApplication::setOrganizationName("deepin");
    QLoggingCategory::setFilterRules("*.debug=false");
    if (appForceStderrLogging) {
        forceStderrLogging = true;
    } else if (qEnvironmentVariableIntValue("QT_FORCE_STDERR_LOGGING")) {
        forceStderrLogging = true;
    }
    installMessageHandler();
    catchUnixSignals({ SIGTERM, SIGQUIT, SIGINT, SIGHUP });
}

void installMessageHandler()
{
    qInstallMessageHandler(linglong_message_handler);
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

} // namespace linglong::utils::global
