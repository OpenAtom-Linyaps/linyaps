/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>
#include <utility>

namespace linglong::utils::error::details {

class ErrorImpl
{
public:
    ErrorImpl(const char *file,
              int line,
              int code,
              std::string msg,
              std::unique_ptr<ErrorImpl> cause = nullptr)
        : _code(code)
        , _msg(std::move(msg))
        , _file(file)
        , _line(line)
        , cause(std::move(cause))
    {
    }

    [[nodiscard]] auto code() const -> int { return _code; };

    [[nodiscard]] auto message() const -> std::string
    {
        std::string msg;
        for (const ErrorImpl *err = this; err != nullptr; err = err->cause.get()) {
            if (!msg.empty()) {
                msg += "\n";
            }
            msg += err->_file + ":" + std::to_string(err->_line) + " " + err->_msg;
        }
        return msg;
    }

private:
    int _code;
    std::string _msg;
    std::string _file;
    int _line;
    std::unique_ptr<ErrorImpl> cause;
};

} // namespace linglong::utils::error::details
