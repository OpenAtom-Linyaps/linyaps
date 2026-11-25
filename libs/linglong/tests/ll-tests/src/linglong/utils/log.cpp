// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/log/log.h"

#include <gtest/gtest.h>

#include "linglong/utils/unique_fd.h"

#include <string>

#include <fcntl.h>
#include <unistd.h>

#if HAVE_SYSTEMD
#  include <cstdarg>
#  include <map>

#  include <syslog.h>
#endif

using namespace linglong::utils::log;

#if HAVE_SYSTEMD
struct MockJournalData
{
    bool called = false;
    std::map<std::string, std::string> fields;

    void reset()
    {
        called = false;
        fields.clear();
    }
};

namespace {
MockJournalData mock_journal_data;
} // namespace

extern "C" int sd_journal_send_with_location(
  const char *file, const char *line, const char *func, const char *format, ...)
{
    mock_journal_data.called = true;
    mock_journal_data.fields["CODE_FILE"] =
      file ? std::string(file).substr(10) : ""; // "CODE_FILE=" is 10 chars
    mock_journal_data.fields["CODE_LINE"] =
      line ? std::string(line).substr(10) : ""; // "CODE_LINE=" is 10 chars
    mock_journal_data.fields["CODE_FUNC"] = func ? func : "";

    va_list args;
    va_start(args, format);

    // systemd-journald 使用 key=value 格式的字符串
    // 我们解析这些参数并存入 map
    const char *p = format;
    while (p != NULL) {
        std::string entry(p);
        size_t eq_pos = entry.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = entry.substr(0, eq_pos);
            std::string value_format = entry.substr(eq_pos + 1);

            if (value_format == "%s") {
                mock_journal_data.fields[key] = va_arg(args, char *);
            } else if (value_format == "%i") {
                mock_journal_data.fields[key] = std::to_string(va_arg(args, int));
            }
        }
        p = va_arg(args, const char *);
    }

    va_end(args);
    return 0; // success
}
#endif

class StderrRedirector
{
public:
    bool init()
    {
        original_stderr_.reset(dup(STDERR_FILENO));
        if (!original_stderr_) {
            return false;
        }
        int pipe_fds[2];
        if (pipe2(pipe_fds, O_DIRECT | O_NONBLOCK) == -1) {
            return false;
        }
        read_fd_.reset(pipe_fds[0]);
        linglong::utils::fd::UniqueFd write_fd(pipe_fds[1]);
        if (dup2(write_fd.get(), STDERR_FILENO) == -1) {
            return false;
        }
        return true;
    }

    ~StderrRedirector()
    {
        if (original_stderr_) {
            dup2(original_stderr_.get(), STDERR_FILENO);
        }
    }

    std::string getOutput()
    {
        char buffer[1024];
        std::string output;
        ssize_t bytes_read;
        while ((bytes_read = read(read_fd_.get(), buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            output += buffer;
            break;
        }
        return output;
    }

private:
    linglong::utils::fd::UniqueFd original_stderr_;
    linglong::utils::fd::UniqueFd read_fd_;
};

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
#if HAVE_SYSTEMD
        mock_journal_data.reset();
#endif
    }

    Logger logger_;
    Logger::LoggerContext context_ = { "test.cpp", 123, "test_func" };
};

TEST_F(LoggerTest, LogBelowLevel_ShouldBeIgnored)
{
    logger_.setLogLevel(LogLevel::Info);
    logger_.setLogBackend(LogBackend::Console | LogBackend::Journal);
    StderrRedirector redirector;
    ASSERT_TRUE(redirector.init());

    logger_.log(context_, LogLevel::Debug, "This is a debug message.");

    EXPECT_TRUE(redirector.getOutput().empty());
#if HAVE_SYSTEMD
    EXPECT_FALSE(mock_journal_data.called);
#endif
}

TEST_F(LoggerTest, LogToConsoleOnly)
{
    logger_.setLogLevel(LogLevel::Info);
    logger_.setLogBackend(LogBackend::Console);

    std::string captured_output;
    {
        StderrRedirector redirector;
        ASSERT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Warning, "Warning: {} happened.", "something");
        captured_output = redirector.getOutput();
    }

    // 验证：控制台输出了正确格式化的消息
    EXPECT_EQ("Warning: something happened.\n", captured_output);
    // 验证：Journald 未被调用
#if HAVE_SYSTEMD
    EXPECT_FALSE(mock_journal_data.called);
#endif
}

#if HAVE_SYSTEMD
TEST_F(LoggerTest, LogToJournalOnly)
{
    logger_.setLogLevel(LogLevel::Info);
    logger_.setLogBackend(LogBackend::Journal);

    logger_.log(context_, LogLevel::Error, "Error code: {}", 404);

    // 验证：Journald 被调用
    ASSERT_TRUE(mock_journal_data.called);
    // 验证：传递给 Journald 的字段是正确的
    EXPECT_EQ(mock_journal_data.fields["CODE_FILE"], "test.cpp");
    EXPECT_EQ(mock_journal_data.fields["CODE_LINE"], "123");
    EXPECT_EQ(mock_journal_data.fields["CODE_FUNC"], "test_func");
    EXPECT_EQ(mock_journal_data.fields["MESSAGE"], "Error code: 404");
    EXPECT_EQ(mock_journal_data.fields["PRIORITY"], std::to_string(static_cast<int>(LOG_ERR)));
}
#else
TEST_F(LoggerTest, JournalFallsBackToConsole)
{
    logger_.setLogLevel(LogLevel::Info);
    logger_.setLogBackend(LogBackend::Journal);
    StderrRedirector redirector;
    ASSERT_TRUE(redirector.init());
    logger_.log(context_, LogLevel::Error, "Error code: {}", 404);
    EXPECT_EQ(redirector.getOutput(), "Error code: 404\n");
}
#endif

TEST_F(LoggerTest, LogToBothConsoleAndJournal)
{
    logger_.setLogLevel(LogLevel::Debug);
    logger_.setLogBackend(LogBackend::Console | LogBackend::Journal);

    std::string captured_output;
    {
        StderrRedirector redirector;
        ASSERT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Debug, "Detailed debug info.");
        captured_output = redirector.getOutput();
    }

    // 验证控制台输出
    EXPECT_EQ("Detailed debug info.\n", captured_output);

    // 验证 Journald 输出
#if HAVE_SYSTEMD
    ASSERT_TRUE(mock_journal_data.called);
    EXPECT_EQ(mock_journal_data.fields["MESSAGE"], "Detailed debug info.");
    EXPECT_EQ(mock_journal_data.fields["PRIORITY"], std::to_string(static_cast<int>(LOG_DEBUG)));
#endif
}

TEST_F(LoggerTest, LogWithNoBackend_ShouldDoNothing)
{
    logger_.setLogLevel(LogLevel::Debug);
    logger_.setLogBackend(LogBackend::None);

    std::string captured_output;
    {
        StderrRedirector redirector;
        ASSERT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Error, "This should not be seen.");
        captured_output = redirector.getOutput();
    }

    // 验证：没有任何输出
    EXPECT_TRUE(captured_output.empty());
#if HAVE_SYSTEMD
    EXPECT_FALSE(mock_journal_data.called);
#endif
}

TEST_F(LoggerTest, MessageFormattingWorksCorrectly)
{
    logger_.setLogBackend(LogBackend::Console | LogBackend::Journal);
    logger_.setLogLevel(LogLevel::Info);
    StderrRedirector redirector;
    ASSERT_TRUE(redirector.init());

    const char *name = "world";
    logger_.log(context_, LogLevel::Info, "Hello, {}! The number is {}.", name, 42);

    EXPECT_EQ(redirector.getOutput(), "Hello, world! The number is 42.\n");
#if HAVE_SYSTEMD
    ASSERT_TRUE(mock_journal_data.called);
    EXPECT_EQ(mock_journal_data.fields["MESSAGE"], "Hello, world! The number is 42.");
#endif
}

TEST_F(LoggerTest, LogFormat)
{
    logger_.setLogLevel(LogLevel::Debug);
    logger_.setLogBackend(LogBackend::Console);

    std::string captured_output;
    {
        StderrRedirector redirector;
        ASSERT_TRUE(redirector.init());
        std::string message = "message is {}";
        logger_.log(context_, LogLevel::Debug, "Debug: {}", message);
        captured_output = redirector.getOutput();
    }

    EXPECT_EQ("Debug: message is {}\n", captured_output);
}
