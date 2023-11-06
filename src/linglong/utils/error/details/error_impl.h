/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_ERROR_DETAILS_H_
#define LINGLONG_UTILS_ERROR_DETAILS_H_

#include "QJsonDocument"
#include "QJsonObject"
#include "QMessageLogContext"
#include "QString"
#include "QStringBuilder"

#include <memory>
#include <utility>

namespace linglong::utils::error::details {

class ErrorImpl
{
public:
    ErrorImpl(const char *file,
              int line,
              const char *function,
              const char *category,
              const int &code,
              QString msg,
              std::unique_ptr<ErrorImpl> cause = nullptr)
        : context(file, line, function, category)
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
            msg += QString("%1:%2\nin %3\n\t[code = %4]\n\t%5\n")
                     .arg(err->context.file)
                     .arg(err->context.line)
                     .arg(err->context.function)
                     .arg(err->_code)
                     .arg(err->msg);
        }
        return msg;
    }

    [[nodiscard]] auto toJSON() const -> QJsonDocument
    {
        QJsonObject obj;
        obj["code"] = this->_code;
        obj["message"] = this->message();
        return QJsonDocument(obj);
    };

private:
    QMessageLogContext context;
    int _code;
    QString msg;
    std::unique_ptr<ErrorImpl> cause;
};

} // namespace linglong::utils::error::details

#endif
