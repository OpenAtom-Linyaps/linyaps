/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/details/error_impl.h"

#include <glib.h>
#include <tl/expected.hpp>

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageLogContext>
#include <QString>
#include <QStringBuilder>

#include <memory>
#include <string>
#include <utility>

namespace linglong::utils::error {

enum class ErrorCode : int {
    Failed = -1, // 通用失败错误码
    Success = 0, // 成功
    /* 通用错误层 */
    Unknown = 1000,               // 未知错误
    AppNotFoundFromRemote = 1001, // 从远程找不到对应应用
    AppNotFoundFromLocal = 1002,  // 从本地找不到对应应用

    /* 安装 */
    AppInstallFailed = 2001,                // 安装失败
    AppInstallNotFoundFromRemote = 2002,    // 远程不存在对应应用
    AppInstallAlreadyInstalled = 2003,      // 本地已安装相同版本的应用
    AppInstallNeedDowngrade = 2004,         // 安装app需要降级
    AppInstallModuleNoVersion = 2005,       // 安装模块时不允许指定版本
    AppInstallModuleRequireAppFirst = 2006, // 安装模块时需要先安装应用
    AppInstallModuleAlreadyExists = 2007,   // 安装模块时已存在相同版本的模块
    AppInstallArchNotMatch = 2008,          // 安装app的架构不匹配
    AppInstallModuleNotFound = 2009,        // 远程不存在对应模块
    AppInstallErofsNotFound = 2010,         // erofs解压命令不存在
    /* 卸载 */
    AppUninstallFailed = 2101,            // 卸载失败
    AppUninstallNotFoundFromLocal = 2102, // 本地不存在对应应用
    AppUninstallAppIsRunning = 2103,      // 卸载的app正在运行
    LayerCompatibilityError = 2104,       // 找不到兼容的layer
    /* 升级 */
    AppUpgradeFailed = 2201,          // 升级失败
    AppUpgradeNotFound = 2202,        // 本地不存在对应应用
    AppUpgradeLatestInstalled = 2203, // 已安装最新版本

    /* 网络 */
    NetworkError = 3001, // 网络错误
};

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

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    const ErrorCode &code) -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          static_cast<int>(code),
                                                          trace_msg + ": " + msg,
                                                          nullptr));
    }

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
                                                          trace_msg + ": " + qfile.fileName() + ": "
                                                            + qfile.errorString(),
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
        QString new_msg = msg;
        if (e != nullptr) {
            new_msg.append(
              QString{ " error code:%1, message:%2" }.arg(QString::number(e->code), e->message));
        }
        return Err(file, line, trace_msg, new_msg);
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const char *msg,
                    const std::system_error &e) -> Error
    {
        return Err(file, line, trace_msg, msg, e, e.code().value());
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const QString &msg,
                    const std::system_error &e) -> Error
    {
        return Err(file, line, trace_msg, msg, e, e.code().value());
    }

    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    const std::string &msg,
                    const std::system_error &e) -> Error
    {
        return Err(file, line, trace_msg, msg.c_str(), e, e.code().value());
    }

    template <typename Value>
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

    template <typename Value>
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

    static auto
    Err(const char *file, int line, const QString &trace_msg, const QString &msg, Error &&cause)
      -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          cause.code(),
                                                          trace_msg + ": " + msg,
                                                          std::move(cause.pImpl)));
    }

    static auto Err(const char *file, int line, const QString &trace_msg, Error &&cause) -> Error
    {

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          "default",
                                                          cause.code(),
                                                          trace_msg,
                                                          std::move(cause.pImpl)));
    }

    template <typename Value>
    static auto Err(const char *file,
                    int line,
                    const QString &trace_msg,
                    tl::expected<Value, std::exception_ptr> &&cause,
                    int code = -1) -> Error
    {
        Q_ASSERT(!cause.has_value());

        return Err(file, line, trace_msg, cause.error(), code);
    }

    template <typename Value>
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

template <typename Value>
using Result = tl::expected<Value, Error>;

} // namespace linglong::utils::error

// Use this macro to define trace message at the begining of function
// 支持QString, std::string, const char*
#define LINGLONG_TRACE(message)                                    \
    QString linglong_trace_message = [](auto &&msg) {              \
        using val_t = decltype(msg);                               \
        if constexpr (std::is_convertible_v<val_t, std::string>) { \
            return QString::fromStdString(msg);                    \
        } else {                                                   \
            return msg;                                            \
        }                                                          \
    }(message);

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
                                                        std::move((_1)) /*NOLINT*/))

// std::move is used for Result<Value>
#define LINGLONG_ERR_2(_1, _2) /*NOLINT*/                                       \
    tl::unexpected(::linglong::utils::error::Error::Err(QT_MESSAGELOG_FILE,     \
                                                        QT_MESSAGELOG_LINE,     \
                                                        linglong_trace_message, \
                                                        (_1),                   \
                                                        std::move((_2)) /*NOLINT*/))

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

#define LINGLONG_ERRV(...) /*NOLINT*/ LINGLONG_ERR(__VA_ARGS__).value()

// https://github.com/AD-Vega/qarv/issues/22#issuecomment-1012011346
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static auto endl = ::endl;
}
#endif

inline QDebug operator<<(QDebug debug, const linglong::utils::error::Error &err)
{
    debug.noquote().nospace() << "[code " << err.code() << " ] message:" << Qt::endl
                              << "\t" << err.message().replace("\n", "\n\t");
    return debug;
}
