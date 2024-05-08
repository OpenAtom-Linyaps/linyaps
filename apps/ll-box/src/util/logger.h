/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_LOGGER_H_
#define LINGLONG_BOX_SRC_UTIL_LOGGER_H_

#include "util.h"

#include <sys/syslog.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

#include <unistd.h>

namespace linglong {
namespace util {
std::string errnoString();
std::string RetErrString(int);
std::string GetPidnsPid();

class Logger
{
public:
    enum Level {
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
    };

    explicit Logger(Level l, const char *fn, int line)
        : level(l)
        , function(fn)
        , line(line){};

    ~Logger()
    {
        if (level < LOGLEVEL) {
            return;
        }

        std::string prefix;
        auto pid_ns = GetPidnsPid();
        int syslogLevel = LOG_DEBUG;
        switch (level) {
        case Debug:
            syslogLevel = LOG_DEBUG;
            break;
        case Info:
            syslogLevel = LOG_INFO;
            break;
        case Warning:
            syslogLevel = LOG_WARNING;
            break;
        case Error:
            syslogLevel = LOG_ERR;
            break;
        case Fatal:
            syslogLevel = LOG_ERR;
            break;
        }
        syslog(syslogLevel, "%s|%s:%d %s", pid_ns.c_str(), function, line, ss.str().c_str());

        switch (level) {
        case Debug:
            prefix = "[DBG |";
            std::cout << prefix << " " << pid_ns << " | " << function << ":" << line << " ] "
                      << ss.str() << std::endl;
            break;
        case Info:
            prefix = "[IFO |";
            std::cout << "\033[1;96m";
            std::cout << prefix << " " << pid_ns << " | " << function << ":" << line << " ] "
                      << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Warning:
            prefix = "[WAN |";
            std::cout << "\033[1;93m";
            std::cout << prefix << " " << pid_ns << " | " << function << ":" << line << " ] "
                      << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Error:
            prefix = "[ERR |";
            std::cout << "\033[1;31m";
            std::cout << prefix << " " << pid_ns << " | " << function << ":" << line << " ] "
                      << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Fatal:
            prefix = "[FAL |";
            std::cout << "\033[1;91m";
            std::cout << prefix << " " << pid_ns << " | " << function << ":" << line << " ] "
                      << ss.str();
            std::cout << "\033[0m" << std::endl;
            exit(-1);
            break;
        }
    }

    template<class T>
    Logger &operator<<(const T &x)
    {
        ss << x << " ";
        return *this;
    }

private:
    static Level LOGLEVEL;
    Level level = Debug;
    const char *function;
    int line;
    std::ostringstream ss;
};
} // namespace util
} // namespace linglong

#define logDbg() (linglong::util::Logger(linglong::util::Logger::Debug, __FUNCTION__, __LINE__))
#define logWan() (linglong::util::Logger(linglong::util::Logger::Warning, __FUNCTION__, __LINE__))
#define logInf() (linglong::util::Logger(linglong::util::Logger::Info, __FUNCTION__, __LINE__))
#define logErr() (linglong::util::Logger(linglong::util::Logger::Error, __FUNCTION__, __LINE__))
#define logFal() (linglong::util::Logger(linglong::util::Logger::Fatal, __FUNCTION__, __LINE__))

#endif /* LINGLONG_BOX_SRC_UTIL_LOGGER_H_ */
