/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_ERROR_H_
#define LINGLONG_UTILS_ERROR_H_

#include "linglong/utils/error/details/error_impl.h"

#include <glib.h>
#include <tl/expected.hpp>

#include <QFile>
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

    static auto
    Err(const char *file, int line, const QString &trace_msg, const QString &msg, int code = -1)
      -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          code,
                                                          trace_msg + ": " + msg,
                                                          nullptr));
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    const QFile &qfile) -> Error
    {
        return Error(
          std::make_unique<details::ErrorImpl>(file,
                                               line,
                                               "default",
                                               qfile.error(),
                                               trace_msg + ": " + msg + ": " + qfile.errorString(),
                                               nullptr));
    }

    static auto Err(const char *file, int line, const QString &trace_msg, const QFile &qfile)
      -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          qfile.error(),
                                                          trace_msg + ": " + qfile.errorString(),
                                                          nullptr));
    }

    static auto
    Err(const char *file, int line, const QString &trace_msg, std::exception_ptr err, int code = -1)
      -> Error
    {
        QString what = trace_msg + ": ";
        try {
            std::rethrow_exception(std::move(err));
        } catch (const std::exception &e) {
            what += e.what();
        } catch (...) {
            what += "unknown";
        }

        return Error(
          std::make_unique<details::ErrorImpl>(file, line, "default", code, what, nullptr));
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    std::exception_ptr err,
                    int code = -1) -> Error
    {
        QString what = trace_msg + ": " + msg + ": ";
        try {
            std::rethrow_exception(std::move(err));
        } catch (const std::exception &e) {
            what += e.what();
        } catch (...) {
            what += "unknown";
        }

        return Error(
          std::make_unique<details::ErrorImpl>(file, line, "default", code, what, nullptr));
    }

    static auto Err(const char *file, int line, const QString &trace_msg, const std::exception &e)
      -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          -1,
                                                          trace_msg + ": " + e.what(),
                                                          nullptr));
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    const std::exception &e,
                    int code = -1) -> Error
    {
        QString what = trace_msg + ": " + msg + ": " + e.what();

        return Error(
          std::make_unique<details::ErrorImpl>(file, line, "default", code, what, nullptr));
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    GError const *const e) -> Error
    {
        return Err(file, line, trace_msg, msg + ": " + e->message, e->code);
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    const std::system_error &e) -> Error
    {
        return Err(file, line, trace_msg, msg, e, e.code().value());
    }

    template<typename Value>
    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    tl::expected<Value, Error> &&cause) -> Error
    {
        Q_ASSERT(!cause.has_value());

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          cause.error().code(),
                                                          trace_msg + ": " + msg,
                                                          std::move(cause.error().pImpl)));
    }

    template<typename Value>
    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    tl::expected<Value, Error> &&cause) -> Error
    {
        Q_ASSERT(!cause.has_value());

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          cause.error().code(),
                                                          trace_msg,
                                                          std::move(cause.error().pImpl)));
    }

    template<typename Value>
    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    tl::expected<Value, std::exception_ptr> &&cause,
                    int code = -1) -> Error
    {
        Q_ASSERT(!cause.has_value());

        return Err(file, line, trace_msg, cause.error(), code);
    }

    template<typename Value>
    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    tl::expected<Value, std::exception_ptr> &&cause) -> Error
    {
        Q_ASSERT(!cause.has_value());

        return Err(file, line, trace_msg, msg, cause.error());
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

// Use this macro to define trace message at the begining of function
#define LINGLONG_TRACE(message) QString linglong_trace_message = message;

// Use this macro to create new error or wrap an existing error
// LINGLONG_ERR(message, code =-1)
// LINGLONG_ERR(message, /* const QFile& */)
// LINGLONG_ERR(/* const QFile& */)
// LINGLONG_ERR(message, /* std::exception_ptr */, code=-1)
// LINGLONG_ERR(/* std::exception_ptr */)
// LINGLONG_ERR(message, /* const std::exception & */, code=-1)
// LINGLONG_ERR(/* const std::exception & */)
// LINGLONG_ERR(message, /* const std::system_exception & */)
// LINGLONG_ERR(message, /* GError* */)
// LINGLONG_ERR(message, /* Result<Value>&& */)
// LINGLONG_ERR(/* Result<Value>&& */)
// LINGLONG_ERR(message, /* tl::expected<Value,std::exception_ptr>&& */, code=-1)
// LINGLONG_ERR(/* tl::expected<Value,std::exception_ptr>&& */)

#define LINGLONG_ERR_GETMACRO(_1, _2, _3, NAME, ...) /*NOLINT*/ NAME
#define LINGLONG_ERR(...) /*NOLINT*/                                                        \
    LINGLONG_ERR_GETMACRO(__VA_ARGS__, LINGLONG_ERR_3, LINGLONG_ERR_2, LINGLONG_ERR_1, ...) \
    (__VA_ARGS__)

// std::move is used for Result<Value>
#define LINGLONG_ERR_1(_1) /*NOLINT*/                                           \
    tl::unexpected(::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE,     \
                                                        QT_MESSAGELOG_LINE,     \
                                                        linglong_trace_message, \
                                                        std::move((_1))))

// std::move is used for Result<Value>
#define LINGLONG_ERR_2(_1, _2) /*NOLINT*/                                       \
    tl::unexpected(::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE,     \
                                                        QT_MESSAGELOG_LINE,     \
                                                        linglong_trace_message, \
                                                        (_1),                   \
                                                        std::move((_2))))

#define LINGLONG_ERR_3(_1, _2, _3) /*NOLINT*/                                   \
    tl::unexpected(::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE,     \
                                                        QT_MESSAGELOG_LINE,     \
                                                        linglong_trace_message, \
                                                        (_1),                   \
                                                        (_2),                   \
                                                        (_3)))

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
