/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "QJsonDocument"
#include "QJsonObject"
#include "QMessageLogContext"
#include "QString"
#include "QStringBuilder"

#include <memory>
#include <utility>

namespace linglong::utils::error {
extern bool isVerboseMessageEnabled();
}

namespace linglong::utils::error::details {

class ErrorImpl
{
public:
    ErrorImpl(const char *file,
              int line,
              const char *category,
              const int &code,
              QString msg,
              std::unique_ptr<ErrorImpl> cause = nullptr)
        : context(file, line, "unknown", category)
        , _code(code)
        , msg(std::move(msg))
        , cause(std::move(cause))
    {
    }

    [[nodiscard]] auto code() const -> int { return _code; };

    [[nodiscard]] auto message() const -> QString
    {
        QString msg;
        for (const ErrorImpl *err = this; err != nullptr; err = err->cause.get()) {
            if (!msg.isEmpty()) {
                msg += "\n";
            }
            if (linglong::utils::error::isVerboseMessageEnabled()) {
                msg +=
                  QString("%1:%2 %3").arg(err->context.file).arg(err->context.line).arg(err->msg);
            } else {
                msg += err->msg;
            }
        }
        return msg;
    }

private:
    QMessageLogContext context;
    int _code;
    QString msg;
    std::unique_ptr<ErrorImpl> cause;
};

} // namespace linglong::utils::error::details
