/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "log.h"

namespace linglong::utils::log {

void setLogLevel(LogLevel level) noexcept
{
    g_logger.setLogLevel(level);
}

void setLogBackend(LogBackend backend) noexcept
{
    g_logger.setLogBackend(backend);
}

Logger::Logger()
    : logLevel(LogLevel::Info)
    , logBackend(LogBackend::None)
{
}

void Logger::setLogLevel(LogLevel level)
{
    logLevel = level;
}

void Logger::setLogBackend(LogBackend backend)
{
#if !HAVE_SYSTEMD
    if ((backend & LogBackend::Journal) != LogBackend::None) {
        backend = (backend & ~LogBackend::Journal) | LogBackend::Console;
    }
#endif
    logBackend = backend;
}

#if HAVE_SYSTEMD
int Logger::logLevelToSDPriority(LogLevel level) noexcept
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
#endif

} // namespace linglong::utils::log
