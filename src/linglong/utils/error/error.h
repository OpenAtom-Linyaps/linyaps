/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_ERROR_H_
#define LINGLONG_UTILS_ERROR_H_

#include "linglong/utils/error/details/error_impl.h"

#include <tl/expected.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageLogContext>
#include <QString>
#include <QStringBuilder>

#include <memory>
#include <tuple>
#include <utility>

namespace linglong::utils::error {

class Error
{
public:
    Error() = default;

    Error(const Error &) = delete;
    Error(Error &&) = default;
    Error &operator=(const Error &) = delete;
    Error &operator=(Error &&) = default;

    [[nodiscard]] auto code() const { return pImpl->code(); };

    [[nodiscard]] auto message() const { return pImpl->message(); }

    static auto Wrap(const char *file, int line, const QString &msg, Error cause) -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          cause.code(),
                                                          msg,
                                                          std::move(cause.pImpl)));
    }

    static auto Err(const char *file, int line, int code, const QString &msg) -> Error
    {
        return Error(
          std::make_unique<details::ErrorImpl>(file, line, "default", code, msg, nullptr));
    }

    static auto Err(const char *file, int line, std::exception_ptr err) -> Error
    {
        QString what;
        try {
            std::rethrow_exception(err);
        } catch (const std::exception &e) {
            what = e.what();
        } catch (...) {
            what = "Unknown exception caught";
        }
        return Error(
          std::make_unique<details::ErrorImpl>(file, line, "default", -1, what, nullptr));
    }

private:
    explicit Error(std::unique_ptr<details::ErrorImpl> pImpl)
        : pImpl(std::move(pImpl))
    {
    }

    std::unique_ptr<details::ErrorImpl> pImpl;
};

template<typename Value>
using Result = tl::expected<Value, Error>;

} // namespace linglong::utils::error

#define LINGLONG_TRACE_MESSAGE(message) auto linglong_trace_message = message;

#define LINGLONG_EWRAP_1(cause) /*NOLINT*/                                       \
    tl::unexpected(::linglong::utils::error::Error::Wrap(QT_MESSAGELOG_FILE,     \
                                                         QT_MESSAGELOG_LINE,     \
                                                         linglong_trace_message, \
                                                         std::move(cause)))

#define LINGLONG_EWRAP_2(message, cause) /*NOLINT*/                          \
    tl::unexpected(::linglong::utils::error::Error::Wrap(QT_MESSAGELOG_FILE, \
                                                         QT_MESSAGELOG_LINE, \
                                                         message,            \
                                                         std::move(cause)))
#define LINGLONG_EWRAP_GETMACRO(_1, _2, NAME, ...) NAME
#define LINGLONG_EWRAP(...) \
    LINGLONG_EWRAP_GETMACRO(__VA_ARGS__, LINGLONG_EWRAP_2, LINGLONG_EWRAP_1, ...)(__VA_ARGS__)

#define LINGLONG_ERR(code, message) /*NOLINT*/ \
    tl::unexpected(                            \
      ::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, code, message))

#define LINGLONG_GERR(gErr) /*NOLINT*/                                      \
    tl::unexpected(::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE, \
                                                        QT_MESSAGELOG_LINE, \
                                                        gErr->code,         \
                                                        gErr->message))

#define LINGLONG_SERR(stdErr) /*NOLINT*/ \
    tl::unexpected(                      \
      ::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, stdErr))

#define LINGLONG_OK \
    {               \
    }

inline QDebug operator<<(QDebug debug, const linglong::utils::error::Error &err)
{
    debug.noquote().nospace() << "[code " << err.code() << " ] message:" << Qt::endl
                              << "\t" << err.message().replace("\n", "\n\t");
    return debug;
}

#endif // LINGLONG_UTILS_ERROR_H_
