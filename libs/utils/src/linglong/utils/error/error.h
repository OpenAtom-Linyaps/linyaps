/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/details/error_impl.h"

#include <tl/expected.hpp>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace linglong::utils::error {

enum class ErrorCode : int {
    Failed = -1, // 通用失败错误码
    Success = 0, // 成功
    Canceled = 1,

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
    AppInstallUnsupportedFileFormat = 2011, // 不支持的文件格式
    /* 卸载 */
    AppUninstallFailed = 2101,            // 卸载失败
    AppUninstallNotFoundFromLocal = 2102, // 本地不存在对应应用
    AppUninstallAppIsRunning = 2103,      // 卸载的app正在运行
    LayerCompatibilityError = 2104,       // 找不到兼容的layer
    AppUninstallMultipleVersions = 2105,
    AppUninstallBaseOrRuntime = 2106,
    /* 升级 */
    AppUpgradeFailed = 2201,        // 升级失败
    AppUpgradeLocalNotFound = 2202, // 本地不存在对应应用
    /* 网络 */
    NetworkError = 3001, // 网络错误

    /* fuzzy reference */
    InvalidFuzzyReference = 4001,
    UnknownArchitecture = 4002,
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
                    const std::string &trace_msg,
                    const std::string &msg,
                    int code = -1) -> Error
    {
        return Error(
          std::make_unique<details::ErrorImpl>(file, line, code, trace_msg, msg, nullptr));
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    const ErrorCode &code) -> Error
    {
        return Err(file, line, trace_msg, msg, static_cast<int>(code));
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    std::exception_ptr err,
                    int code = -1) -> Error
    {
        std::string what;
        try {
            std::rethrow_exception(std::move(err));
        } catch (const std::exception &e) {
            what = e.what();
        } catch (...) {
            what = "unknown";
        }

        return Err(file, line, trace_msg, what, code);
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    std::exception_ptr err,
                    int code = -1) -> Error
    {
        std::string what = msg + ": ";
        try {
            std::rethrow_exception(std::move(err));
        } catch (const std::exception &e) {
            what += e.what();
        } catch (...) {
            what += "unknown";
        }

        return Err(file, line, trace_msg, what, code);
    }

    static auto
    Err(const char *file, int line, const std::string &trace_msg, const std::exception &e) -> Error
    {
        return Err(file, line, trace_msg, e.what());
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    const std::exception &e) -> Error
    {
        std::string what = msg + ": " + e.what();
        return Err(file, line, trace_msg, what);
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    const std::system_error &e) -> Error
    {
        std::string what = msg + ": " + e.what();
        return Err(file, line, trace_msg, what, e.code().value());
    }

    template <typename Value>
    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    tl::expected<Value, Error> &&cause) -> Error
    {
        assert(!cause.has_value());

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          cause.error().code(),
                                                          trace_msg,
                                                          msg,
                                                          std::move(cause.error().pImpl)));
    }

    template <typename Value>
    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    tl::expected<Value, Error> &&cause) -> Error
    {
        assert(!cause.has_value());

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          cause.error().code(),
                                                          trace_msg,
                                                          std::nullopt,
                                                          std::move(cause.error().pImpl)));
    }

    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    const std::string &msg,
                    Error &&cause) -> Error
    {
        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          cause.code(),
                                                          trace_msg,
                                                          msg,
                                                          std::move(cause.pImpl)));
    }

    static auto Err(const char *file, int line, const std::string &trace_msg, Error &&cause)
      -> Error
    {

        return Error(std::make_unique<details::ErrorImpl>(file,
                                                          line,
                                                          cause.code(),
                                                          trace_msg,
                                                          std::nullopt,
                                                          std::move(cause.pImpl)));
    }

    template <typename Value>
    static auto Err(const char *file,
                    int line,
                    const std::string &trace_msg,
                    tl::expected<Value, std::exception_ptr> &&cause,
                    int code = -1) -> Error
    {
        assert(!cause.has_value());

        return Err(file, line, trace_msg, cause.error(), code);
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

// Use this macro to set trace message for the current scope
#define LINGLONG_TRACE(message) const std::string _linglong_trace_message = message;

// Use this macro to create new error or wrap an existing error
// LINGLONG_ERR(message, code =-1)
// LINGLONG_ERR(message, /* std::exception_ptr */, code=-1)
// LINGLONG_ERR(/* std::exception_ptr */)
// LINGLONG_ERR(message, /* const std::exception & */, code=-1)
// LINGLONG_ERR(/* const std::exception & */)
// LINGLONG_ERR(message, /* const std::system_exception & */)
// LINGLONG_ERR(message, /* Result<Value>&& */)
// LINGLONG_ERR(/* Result<Value>&& */)
// LINGLONG_ERR(message, /* tl::expected<Value,std::exception_ptr>&& */, code=-1)
// LINGLONG_ERR(/* tl::expected<Value,std::exception_ptr>&& */)

#define LINGLONG_ERR_GETMACRO(_1, _2, _3, NAME, ...) /*NOLINT*/ NAME
#define LINGLONG_ERR(...) /*NOLINT*/                                                        \
    LINGLONG_ERR_GETMACRO(__VA_ARGS__, LINGLONG_ERR_3, LINGLONG_ERR_2, LINGLONG_ERR_1, ...) \
    (__VA_ARGS__)

// std::move is used for Result<Value>
#define LINGLONG_ERR_1(_1) /*NOLINT*/                                            \
    tl::unexpected(::linglong::utils::error::Error::Err(__FILE__,                \
                                                        __LINE__,                \
                                                        _linglong_trace_message, \
                                                        std::move((_1)) /*NOLINT*/))

// std::move is used for Result<Value>
#define LINGLONG_ERR_2(_1, _2) /*NOLINT*/                                        \
    tl::unexpected(::linglong::utils::error::Error::Err(__FILE__,                \
                                                        __LINE__,                \
                                                        _linglong_trace_message, \
                                                        (_1),                    \
                                                        std::move((_2)) /*NOLINT*/))

#define LINGLONG_ERR_3(_1, _2, _3) /*NOLINT*/                                    \
    tl::unexpected(::linglong::utils::error::Error::Err(__FILE__,                \
                                                        __LINE__,                \
                                                        _linglong_trace_message, \
                                                        (_1),                    \
                                                        (_2),                    \
                                                        (_3)))

#define LINGLONG_OK \
    {               \
    }

#define LINGLONG_ERRV(...) /*NOLINT*/ LINGLONG_ERR(__VA_ARGS__).value()
