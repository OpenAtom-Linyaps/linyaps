/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#define FMT_HEADER_ONLY
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
#define LOGCTX LOGNS::Logger::LoggerContext(__FILE__, __LINE__, __func__)
#define LogD(fmt, args...) LOGNS::g_logger.log(LOGCTX, LOGLV::Debug, fmt, ##args)
#define LogI(fmt, args...) LOGNS::g_logger.log(LOGCTX, LOGLV::Info, fmt, ##args)
#define LogW(fmt, args...) LOGNS::g_logger.log(LOGCTX, LOGLV::Warning, fmt, ##args)
#define LogE(fmt, args...) LOGNS::g_logger.log(LOGCTX, LOGLV::Error, fmt, ##args)
#define LogF(fmt, args...) LOGNS::g_logger.log(LOGCTX, LOGLV::Fatal, fmt, ##args)

namespace linglong::utils::log {

enum class LogBackend : uint8_t {
    None = 0,
    Console = 1 << 0,
    Journal = 1 << 1,
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

        std::string file;
        int line;
        std::string function;
    };

    Logger();
    ~Logger();

    void setLogLevel(LogLevel level);
    void setLogBackend(LogBackend backend);

    template <typename... Args>
    void log(const LoggerContext &context, LogLevel level, fmt::string_view fmt, Args &&...args)
    {
        if (logLevel < level || logBackend == LogBackend::None) {
            return;
        }

        auto message = fmt::vformat(fmt, fmt::make_format_args(args...));

        if ((logBackend & LogBackend::Console) != LogBackend::None) {
            fmt::print(stderr, "{}\n", message);
        }

        if ((logBackend & LogBackend::Journal) != LogBackend::None) {
            sd_journal_send_with_location(fmt::format("CODE_FILE={}", context.file).c_str(),
                                          fmt::format("CODE_LINE={}", context.line).c_str(),
                                          context.function.c_str(),
                                          "MESSAGE=%s",
                                          message.c_str(),
                                          "PRIORITY=%i",
                                          logLevelToSDPriority(level),
                                          NULL);
        }
    }

private:
    int logLevelToSDPriority(LogLevel level);

    LogLevel logLevel;
    LogBackend logBackend;
};

void setLogLevel(LogLevel level);
void setLogBackend(LogBackend backend);

extern Logger g_logger;

} // namespace linglong::utils::log
