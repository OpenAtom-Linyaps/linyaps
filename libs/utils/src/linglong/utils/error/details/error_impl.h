/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace linglong::utils::error::details {

class ErrorImpl
{
public:
    ErrorImpl(const char *file,
              int line,
              int code,
              std::string trace_msg,
              std::optional<std::string> msg,
              std::unique_ptr<ErrorImpl> cause = nullptr)
        : _code(code)
        , _msg(std::move(msg))
        , _file(file)
        , _line(line)
        , cause(std::move(cause))
        , _trace_msg(std::move(trace_msg))
    {
        backtrace = (getenv("LINYAPS_BACKTRACE") != nullptr);
    }

    [[nodiscard]] auto code() const -> int { return _code; };

    [[nodiscard]] auto message() const -> std::string
    {
        std::string msg;
        for (const ErrorImpl *err = this; err != nullptr; err = err->cause.get()) {
            if (!msg.empty()) {
                msg += "\n";
            }
            if (backtrace) {
                auto trace_msg = err->_trace_msg;
                if (err->_msg) {
                    trace_msg += ": " + *err->_msg;
                }

                msg += fmt::format("{}:{} {}", err->_file, err->_line, trace_msg);
            } else {
                if (err->_msg) {
                    msg = *err->_msg;
                    break;
                }
            }
        }
        return msg;
    }

private:
    int _code;
    std::optional<std::string> _msg;
    std::string _file;
    int _line;
    std::unique_ptr<ErrorImpl> cause;
    std::string _trace_msg;
    bool backtrace = false;
};

} // namespace linglong::utils::error::details
