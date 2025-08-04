/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "log.h"

namespace linglong::utils::log {

Logger g_logger;

void setLogLevel(LogLevel level)
{
    g_logger.setLogLevel(level);
}

void setLogBackend(LogBackend backend)
{
    g_logger.setLogBackend(backend);
}

Logger::Logger()
    : logLevel(LogLevel::Info)
{
}

Logger::~Logger() { }

void Logger::setLogLevel(LogLevel level)
{
    logLevel = level;
}

void Logger::setLogBackend(LogBackend backend)
{
    logBackend = backend;
}

int Logger::logLevelToSDPriority(LogLevel level)
{
    switch (level) {
    case LogLevel::Fatal:
        return LOG_CRIT;
    case LogLevel::Error:
        return LOG_ERR;
    case LogLevel::Warning:
        return LOG_WARNING;
    case LogLevel::Info:
        return LOG_INFO;
    case LogLevel::Debug:
        return LOG_DEBUG;
    default:
        return LOG_INFO;
    }
}

} // namespace linglong::utils::log