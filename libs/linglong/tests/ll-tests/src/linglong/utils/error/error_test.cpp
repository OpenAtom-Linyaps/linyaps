/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/strings.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <filesystem>

using namespace linglong::utils::error;

using namespace linglong::common;
using linglong::utils::EnvironmentVariableGuard;

TEST(Error, New)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        return LINGLONG_ERR("message", -1);
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "LINGLONG_ERR")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message")) << msg;
}

TEST(Error, WrapResult)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        auto res = []() -> Result<void> {
            LINGLONG_TRACE("test LINGLONG_ERR");
            auto res = []() -> Result<void> {
                LINGLONG_TRACE("test LINGLONG_ERR");
                auto res = []() -> Result<void> {
                    LINGLONG_TRACE("test LINGLONG_ERR");
                    return LINGLONG_ERR("message1", -1);
                }();
                return LINGLONG_ERR("message2", res);
            }();
            return LINGLONG_ERR(std::string("message3"), res);
        }();
        return LINGLONG_ERR(res);
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "LINGLONG_ERR")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message1")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message2")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message3")) << msg;
}

TEST(Error, WrapError)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        auto res = []() -> Result<void> {
            LINGLONG_TRACE("test LINGLONG_ERR");
            auto res = []() -> Result<void> {
                LINGLONG_TRACE("test LINGLONG_ERR");
                return LINGLONG_ERR("message1", -1);
            }();
            return LINGLONG_ERR("message2", res.error());
        }();
        return LINGLONG_ERR(res.error());
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "LINGLONG_ERR")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message1")) << msg;
    ASSERT_TRUE(strings::contains(msg, "message2")) << msg;
}

TEST(Error, WarpErrorCode)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        return LINGLONG_ERR("app install failed", ErrorCode::AppInstallFailed);
    }();
    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR")) << msg;
    ASSERT_TRUE(strings::contains(msg, "app install failed")) << msg;
    ASSERT_TRUE(res.error().code() == int(ErrorCode::AppInstallFailed)) << msg;
}

TEST(Error, WarpStdErrorCode)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    std::system_error se;
    auto res = [&se]() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        std::error_code ec;
        std::filesystem::create_directory("/no_permission_to_create", ec);
        if (ec) {
            se = std::system_error(ec);
            return LINGLONG_ERR("create directory failed", ec);
        }
        return LINGLONG_OK;
    }();
    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR")) << msg;
    ASSERT_TRUE(strings::contains(msg, "create directory failed")) << msg;
    ASSERT_TRUE(strings::contains(msg, se.what())) << msg;
}

TEST(Error, WarpSystemError)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with std::system_error");
        try {
            throw std::system_error(ENOENT, std::generic_category(), "File not found");
        } catch (const std::system_error &e) {
            return LINGLONG_ERR(std::string("System operation failed"), e);
        }
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with std::system_error")) << msg;
    ASSERT_TRUE(strings::contains(msg, "System operation failed")) << msg;
    ASSERT_TRUE(strings::contains(msg, "File not found")) << msg;
    ASSERT_EQ(res.error().code(), ENOENT);
}

TEST(Error, WarpExceptionPtr)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with std::exception_ptr");
        std::exception_ptr eptr;
        try {
            throw std::runtime_error("Runtime error occurred");
        } catch (...) {
            eptr = std::current_exception();
        }
        return LINGLONG_ERR(eptr);
    }();
    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with std::exception_ptr")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Runtime error occurred")) << msg;

    res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with std::exception_ptr");
        std::exception_ptr eptr;
        try {
            throw std::runtime_error("Runtime error occurred");
        } catch (...) {
            eptr = std::current_exception();
        }
        return LINGLONG_ERR("Exception handling failed", eptr, 500);
    }();

    ASSERT_FALSE(res.has_value());
    msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with std::exception_ptr")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Runtime error occurred")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Exception handling failed")) << msg;
    ASSERT_EQ(res.error().code(), 500);
}

TEST(Error, WarpStdException)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with std::exception");
        std::invalid_argument e("Invalid argument provided");
        return LINGLONG_ERR("Validation failed", e);
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with std::exception")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Validation failed")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Invalid argument provided")) << msg;
}

TEST(Error, WarpNestedError)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        auto innerFn = []() -> Result<void> {
            LINGLONG_TRACE("inner function");
            return LINGLONG_ERR("Inner error", ErrorCode::NetworkError);
        };

        LINGLONG_TRACE("test LINGLONG_ERR with nested error");
        auto innerRes = innerFn();
        if (!innerRes) {
            return LINGLONG_ERR("Outer operation failed", std::move(innerRes));
        }

        return LINGLONG_OK;
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with nested error")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Outer operation failed")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Inner error")) << msg;
    ASSERT_EQ(res.error().code(), static_cast<int>(ErrorCode::NetworkError));
}

TEST(Error, WarpErrorWithCustomCode)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with custom error code");
        return LINGLONG_ERR("Custom error occurred", 12345);
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with custom error code")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Custom error occurred")) << msg;
    ASSERT_EQ(res.error().code(), 12345);
}

TEST(Error, WarpStdStringMessage)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR with std::string message");
        std::string errorMsg = "Standard string error message";
        return LINGLONG_ERR(errorMsg, ErrorCode::AppNotFoundFromRemote);
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_TRUE(strings::contains(msg, "test LINGLONG_ERR with std::string message")) << msg;
    ASSERT_TRUE(strings::contains(msg, "Standard string error message")) << msg;
    ASSERT_EQ(res.error().code(), static_cast<int>(ErrorCode::AppNotFoundFromRemote));
}

TEST(Error, SuccessCase)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("test successful operation");
        // 模拟成功操作
        return LINGLONG_OK;
    }();

    ASSERT_TRUE(res.has_value());
}

TEST(Error, ErrorCodeEnumValues)
{
    EnvironmentVariableGuard guard("LINYAPS_BACKTRACE", "1");
    // 测试各种错误码枚举值
    auto testErrorCode = [](ErrorCode code, const std::string &description) -> Result<void> {
        LINGLONG_TRACE("test error code : " + description);
        return LINGLONG_ERR(description, code);
    };

    // 测试几个重要的错误码
    auto res1 = testErrorCode(ErrorCode::AppInstallFailed, "App installation failed");
    ASSERT_FALSE(res1.has_value());
    ASSERT_EQ(res1.error().code(), static_cast<int>(ErrorCode::AppInstallFailed));

    auto res2 = testErrorCode(ErrorCode::NetworkError, "Network connection failed");
    ASSERT_FALSE(res2.has_value());
    ASSERT_EQ(res2.error().code(), static_cast<int>(ErrorCode::NetworkError));

    auto res3 = testErrorCode(ErrorCode::Success, "This should not happen");
    ASSERT_FALSE(res3.has_value());
    ASSERT_EQ(res3.error().code(), 0); // Success 错误码为0
}

TEST(Error, NoBacktrace)
{
    auto res = []() -> Result<void> {
        LINGLONG_TRACE("trace");
        return LINGLONG_ERR("error");
    }();

    ASSERT_FALSE(res.has_value());
    auto msg = res.error().message();
    ASSERT_FALSE(strings::contains(msg, "trace")) << msg;
    ASSERT_TRUE(strings::contains(msg, "error")) << msg;
}

TEST(Error, DefaultError)
{
    auto res1 = []() -> Result<void> {
        LINGLONG_TRACE("trace 1");
        return LINGLONG_ERR("error 1", 1);
    };

    auto res2 = [&res1]() -> Result<void> {
        LINGLONG_TRACE("trace 2");
        return LINGLONG_ERR(res1());
    }();

    ASSERT_FALSE(res2.has_value());
    auto msg = res2.error().message();
    ASSERT_FALSE(strings::contains(msg, "trace")) << msg;
    ASSERT_TRUE(strings::contains(msg, "error 1")) << msg;
    ASSERT_EQ(res2.error().code(), 1);

    auto res3 = [&res1]() -> Result<void> {
        LINGLONG_TRACE("trace 3");
        return LINGLONG_ERR("error 3", res1());
    }();
    ASSERT_FALSE(res3.has_value());
    msg = res3.error().message();
    ASSERT_FALSE(strings::contains(msg, "trace")) << msg;
    ASSERT_TRUE(strings::contains(msg, "error 3")) << msg;
    ASSERT_EQ(res3.error().code(), 1);
}
