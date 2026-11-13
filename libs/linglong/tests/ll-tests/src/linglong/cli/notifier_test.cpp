/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/terminal_notifier.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace linglong;

api::types::v1::InteractionRequest makeRequest(std::optional<std::vector<std::string>> actions)
{
    return api::types::v1::InteractionRequest{
        .actions = std::move(actions),
        .appName = "org.example.App",
        .body = std::string{ "details" },
        .summary = "install org.example.App",
        .timeout = 0,
    };
}

class ScopedStandardStreams
{
public:
    explicit ScopedStandardStreams(const std::string &input)
        : inputBuffer(input)
    {
        oldIn = std::cin.rdbuf(inputBuffer.rdbuf());
        oldOut = std::cout.rdbuf(outputBuffer.rdbuf());
    }

    ~ScopedStandardStreams()
    {
        std::cin.rdbuf(oldIn);
        std::cout.rdbuf(oldOut);
    }

    std::string output() const { return outputBuffer.str(); }

private:
    std::istringstream inputBuffer;
    std::ostringstream outputBuffer;
    std::streambuf *oldIn{ nullptr };
    std::streambuf *oldOut{ nullptr };
};

TEST(DummyNotifier, requestSelectsDummyAction)
{
    cli::DummyNotifier notifier;
    auto reply = notifier.request(makeRequest(std::vector<std::string>{ "ok", "yes" }));
    ASSERT_TRUE(reply) << reply.error().message();
    ASSERT_TRUE(reply->action.has_value());
    EXPECT_EQ(*reply->action, "dummy");
}

TEST(DummyNotifier, notifySucceeds)
{
    cli::DummyNotifier notifier;
    auto result = notifier.notify(makeRequest(std::nullopt));
    EXPECT_TRUE(result) << result.error().message();
}

TEST(TerminalNotifier, notifyPrintsSummaryAndBody)
{
    cli::TerminalNotifier notifier;
    ScopedStandardStreams streams{ "" };
    auto result = notifier.notify(makeRequest(std::nullopt));
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("install org.example.App"));
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("details"));
}

TEST(TerminalNotifier, notifyOmitsBodyWhenAbsent)
{
    cli::TerminalNotifier notifier;
    auto request = makeRequest(std::nullopt);
    request.body.reset();

    ScopedStandardStreams streams{ "" };
    auto result = notifier.notify(request);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("install org.example.App"));
    EXPECT_THAT(streams.output(), ::testing::Not(::testing::HasSubstr("details")));
}

TEST(TerminalNotifier, requestReturnsSelectedAction)
{
    cli::TerminalNotifier notifier;
    ScopedStandardStreams streams{ "continue\n" };
    auto reply =
      notifier.request(makeRequest(std::vector<std::string>{ "yes", "continue", "no", "cancel" }));
    ASSERT_TRUE(reply) << reply.error().message();
    ASSERT_TRUE(reply->action.has_value());
    EXPECT_EQ(*reply->action, "continue");
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("available actions:"));
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("your choice:"));
}

TEST(TerminalNotifier, requestWithoutActionsPromptsForEnter)
{
    cli::TerminalNotifier notifier;
    ScopedStandardStreams streams{ "" };
    auto reply = notifier.request(makeRequest(std::nullopt));
    ASSERT_TRUE(reply) << reply.error().message();
    EXPECT_FALSE(reply->action.has_value());
    EXPECT_THAT(streams.output(), ::testing::HasSubstr("press enter to continue"));
}

TEST(TerminalNotifier, requestRejectsEmptyActions)
{
    cli::TerminalNotifier notifier;
    ScopedStandardStreams streams{ "" };
    auto reply = notifier.request(makeRequest(std::vector<std::string>{}));
    EXPECT_FALSE(reply);
    EXPECT_THAT(reply.error().message(), ::testing::HasSubstr("empty action"));
}

TEST(TerminalNotifier, requestRejectsOddActionList)
{
    cli::TerminalNotifier notifier;
    ScopedStandardStreams streams{ "" };
    auto reply = notifier.request(makeRequest(std::vector<std::string>{ "yes", "continue", "no" }));
    EXPECT_FALSE(reply);
    EXPECT_THAT(reply.error().message(), ::testing::HasSubstr("invalid action"));
}

} // namespace
