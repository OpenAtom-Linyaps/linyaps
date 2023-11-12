/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/global/initialize.h"

#include <systemd/sd-journal.h>

#include <QCoreApplication>
#include <QDebug>
#include <csignal>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::utils::global {

namespace {
void catchUnixSignals(std::initializer_list<int> quitSignals)
{
    auto handler = [](int sig) -> void {
        qInfo().noquote() << QString("Quit the application by signal(%1).").arg(sig);
        QCoreApplication::quit();
    };

    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    for (auto sig : quitSignals)
        sigaddset(&blocking_mask, sig);

    struct sigaction sa
    {
    };

    sa.sa_handler = handler;
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals)
        sigaction(sig, &sa, nullptr);
}

auto shouldLogToStderr() -> bool
{
    static bool forceStderrLogging = qEnvironmentVariableIntValue("QT_FORCE_STDERR_LOGGING");
    return forceStderrLogging || isatty(STDERR_FILENO);
}

void linglong_message_handler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &message)
{
    QString formattedMessage = qFormatLogMessage(type, context, message);

    if (shouldLogToStderr()) {
        // print nothing if message pattern didn't apply / was empty.
        // (still print empty lines, e.g. because message itself was empty)
        if (formattedMessage.isNull())
            return;

        fprintf(stderr, "%s\n", formattedMessage.toLocal8Bit().constData());
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
} // namespace

void applicationInitializte()
{
    QCoreApplication::setOrganizationName("deepin");
    qInstallMessageHandler(linglong_message_handler);
    catchUnixSignals({ SIGTERM, SIGQUIT, SIGINT, SIGHUP });
    return;
}

} // namespace linglong::utils::global
