/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_ERROR_H_
#define LINGLONG_UTILS_ERROR_H_

#include "QJsonDocument"
#include "QJsonObject"
#include "QMessageLogContext"
#include "QString"
#include "QStringBuilder"
#include "tl/expected.hpp"

#include <memory>
#include <utility>

namespace linglong::utils::error {
namespace {
class error
{
public:
    error(const char *file,
          int line,
          const char *function,
          const char *category,
          const int &code,
          QString msg,
          std::unique_ptr<error> cause = nullptr)
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
        for (const error *err = this; err != nullptr; err = err->cause.get()) {
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
    std::unique_ptr<error> cause;
};
} // namespace

using Error = std::unique_ptr<error>;

namespace {
inline auto
doEWrap(const char *file, int line, const char *function, const QString &msg, Error cause) -> Error
{
    return std::make_unique<error>(file,
                                   line,
                                   function,
                                   "default",
                                   cause->code(),
                                   msg,
                                   std::move(cause));
}

inline auto doErr(const char *file, int line, const char *function, int code, const QString &msg)
  -> Error
{
    return std::make_unique<error>(file, line, function, "default", code, msg, nullptr);
}

} // namespace

template<typename Value>
using Result = tl::expected<Value, Error>;

} // namespace linglong::utils::error

#define EWrap(message, cause) /*NOLINT*/                                 \
    tl::unexpected(::linglong::utils::error::doEWrap(QT_MESSAGELOG_FILE, \
                                                     QT_MESSAGELOG_LINE, \
                                                     QT_MESSAGELOG_FUNC, \
                                                     message,            \
                                                     std::move(cause)))

#define Err(code, message) /*NOLINT*/                                  \
    tl::unexpected(::linglong::utils::error::doErr(QT_MESSAGELOG_FILE, \
                                                   QT_MESSAGELOG_LINE, \
                                                   QT_MESSAGELOG_FUNC, \
                                                   code,               \
                                                   message))

#define Ok \
    {      \
    }

#endif
