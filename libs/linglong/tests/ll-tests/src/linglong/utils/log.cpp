// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/log/log.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <syslog.h>

using namespace linglong::utils::log;

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

static MockJournalData mock_journal_data;

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

class StderrRedirector
{
public:
    StderrRedirector() { init(); }

    bool init()
    {
        original_stderr_ = dup(STDERR_FILENO);
        if (original_stderr_ == -1) {
            return false;
        }
        if (pipe2(pipe_fds_, O_DIRECT | O_NONBLOCK) == -1) {
            return false;
        }
        if (dup2(pipe_fds_[1], STDERR_FILENO) == -1) {
            return false;
        }
        close(pipe_fds_[1]);
        return true;
    }

    ~StderrRedirector()
    {
        dup2(original_stderr_, STDERR_FILENO);
        close(original_stderr_);
    }

    std::string getOutput()
    {
        char buffer[1024];
        std::string output;
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_fds_[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            output += buffer;
            break;
        }
        return output;
    }

private:
    int original_stderr_;
    int pipe_fds_[2];
};

class LoggerTest : public ::testing::Test
{
protected:
    // 每个测试用例开始前执行
    void SetUp() override
    {
        mock_journal_data.reset(); // 重置模拟数据
    }

    // 每个测试用例结束后执行
    void TearDown() override
    {
        // 清理工作
    }

    Logger logger_;
    Logger::LoggerContext context_ = { "test.cpp", 123, "test_func" };
};

TEST_F(LoggerTest, LogBelowLevel_ShouldBeIgnored)
{
    logger_.setLogLevel(LogLevel::Info);

    logger_.log(context_, LogLevel::Debug, "This is a debug message.");

    // 验证：没有任何输出
    EXPECT_FALSE(mock_journal_data.called);
}

TEST_F(LoggerTest, LogToConsoleOnly)
{
    logger_.setLogLevel(LogLevel::Info);
    logger_.setLogBackend(LogBackend::Console);

    std::string captured_output;
    {
        StderrRedirector redirector;
        EXPECT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Warning, "Warning: {} happened.", "something");
        captured_output = redirector.getOutput();
    }

    // 验证：控制台输出了正确格式化的消息
    EXPECT_EQ("Warning: something happened.\n", captured_output);
    // 验证：Journald 未被调用
    EXPECT_FALSE(mock_journal_data.called);
}

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

TEST_F(LoggerTest, LogToBothConsoleAndJournal)
{
    logger_.setLogLevel(LogLevel::Debug);
    logger_.setLogBackend(LogBackend::Console | LogBackend::Journal);

    std::string captured_output;
    {
        StderrRedirector redirector;
        EXPECT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Debug, "Detailed debug info.");
        captured_output = redirector.getOutput();
    }

    // 验证控制台输出
    EXPECT_EQ("Detailed debug info.\n", captured_output);

    // 验证 Journald 输出
    ASSERT_TRUE(mock_journal_data.called);
    EXPECT_EQ(mock_journal_data.fields["MESSAGE"], "Detailed debug info.");
    EXPECT_EQ(mock_journal_data.fields["PRIORITY"], std::to_string(static_cast<int>(LOG_DEBUG)));
}

TEST_F(LoggerTest, LogWithNoBackend_ShouldDoNothing)
{
    logger_.setLogLevel(LogLevel::Debug);
    logger_.setLogBackend(LogBackend::None);

    std::string captured_output;
    {
        StderrRedirector redirector;
        EXPECT_TRUE(redirector.init());
        logger_.log(context_, LogLevel::Error, "This should not be seen.");
        captured_output = redirector.getOutput();
    }

    // 验证：没有任何输出
    EXPECT_TRUE(captured_output.empty());
    EXPECT_FALSE(mock_journal_data.called);
}

TEST_F(LoggerTest, MessageFormattingWorksCorrectly)
{
    logger_.setLogBackend(LogBackend::Journal);
    logger_.setLogLevel(LogLevel::Info);

    const char *name = "world";
    logger_.log(context_, LogLevel::Info, "Hello, {}! The number is {}.", name, 42);

    ASSERT_TRUE(mock_journal_data.called);
    EXPECT_EQ(mock_journal_data.fields["MESSAGE"], "Hello, world! The number is 42.");
}