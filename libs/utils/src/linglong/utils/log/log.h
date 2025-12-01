/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/log/formatter.h"

#include <fmt/format.h>
#include <systemd/sd-journal.h>

/*
log to journal can view by journalctl:
journalctl SYSLOG_IDENTIFIER=ll-cli --no-pager -o json
journalctl SYSLOG_IDENTIFIER=ll-package-manager --no-pager -o json
*/

#define LOGNS linglong::utils::log
#define LOGLV LOGNS::LogLevel
#define LOGCTX LOGNS::Logger::LoggerContext(__FILE__, __LINE__, __PRETTY_FUNCTION__)

#define LogD(...) LOGNS::g_logger.log(LOGCTX, LOGLV::Debug, __VA_ARGS__)
#define LogI(...) LOGNS::g_logger.log(LOGCTX, LOGLV::Info, __VA_ARGS__)
#define LogW(...) LOGNS::g_logger.log(LOGCTX, LOGLV::Warning, __VA_ARGS__)
#define LogE(...) LOGNS::g_logger.log(LOGCTX, LOGLV::Error, __VA_ARGS__)
#define LogF(...) LOGNS::g_logger.log(LOGCTX, LOGLV::Fatal, __VA_ARGS__)

namespace linglong::utils::log {

enum class LogBackend : uint8_t {
    None = 0,
    Console = 1U << 0U,
    Journal = 1U << 1U,
};

inline LogBackend operator|(LogBackend a, LogBackend b)
{
    return static_cast<LogBackend>(std::underlying_type_t<LogBackend>(a)
                                   | std::underlying_type_t<LogBackend>(b));
}

inline LogBackend operator&(LogBackend a, LogBackend b)
{
    return static_cast<LogBackend>(std::underlying_type_t<LogBackend>(a)
                                   & std::underlying_type_t<LogBackend>(b));
}

enum class LogLevel : uint8_t {
    Fatal,
    Error,
    Warning,
    Info,
    Debug,
};

class Logger
{
public:
    class LoggerContext
    {
    public:
        LoggerContext(const char *file, int line, const char *function)
            : file(file)
            , line(line)
            , function(function)
        {
        }

        const char *file;
        int line;
        const char *function;
    };

    Logger();
    ~Logger() = default;

    void setLogLevel(LogLevel level);
    void setLogBackend(LogBackend backend);

    template <typename... Args>
    void log(const LoggerContext &context,
             LogLevel level,
             fmt::format_string<Args...> fmt,
             Args &&...args)
    {
        if (logLevel < level || logBackend == LogBackend::None) {
            return;
        }

        std::string message;
        try {
            message = fmt::format(fmt, std::forward<Args>(args)...);
        } catch (const fmt::format_error &e) {
            message = fmt::format("[format error: {}]", e.what());
        }

        if ((logBackend & LogBackend::Console) != LogBackend::None) {
            fmt::println(stderr, "{}", message);
        }

        if ((logBackend & LogBackend::Journal) != LogBackend::None) {
            fmt::memory_buffer fileBuf;
            fmt::memory_buffer lineBuf;
            fmt::format_to(fmt::appender(fileBuf), "CODE_FILE={}", context.file);
            fmt::format_to(fmt::appender(lineBuf), "CODE_LINE={}", context.line);
            sd_journal_send_with_location(fmt::to_string(fileBuf).c_str(),
                                          fmt::to_string(lineBuf).c_str(),
                                          context.function,
                                          "MESSAGE=%s",
                                          message.c_str(),
                                          "PRIORITY=%i",
                                          logLevelToSDPriority(level),
                                          nullptr);
        }
    }

private:
    static int logLevelToSDPriority(LogLevel level) noexcept;

    LogLevel logLevel;
    LogBackend logBackend;
};

void setLogLevel(LogLevel level) noexcept;
void setLogBackend(LogBackend backend) noexcept;

inline Logger g_logger;

} // namespace linglong::utils::log
