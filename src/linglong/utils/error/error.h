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

#define LINGLONG_EWRAP(message, cause) /*NOLINT*/                            \
    tl::unexpected(::linglong::utils::error::Error::Wrap(QT_MESSAGELOG_FILE, \
                                                         QT_MESSAGELOG_LINE, \
                                                         message,            \
                                                         std::move(cause)))

#define LINGLONG_ERR(code, message) /*NOLINT*/ \
    tl::unexpected(                            \
      ::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, code, message))

#define LINGLONG_GERR(gErr) /*NOLINT*/ \
    tl::unexpected(                            \
      ::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, gErr->code, gErr->message))

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
