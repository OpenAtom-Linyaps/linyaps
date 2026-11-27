/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/architecture.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/utils/error/error.h"

#include <optional>
#include <string>
#include <vector>

using namespace linglong::package;
using linglong::utils::error::ErrorCode;

namespace {

struct ParseRoundTripCase
{
    std::string raw;
    std::string expected;
};

const std::vector<ParseRoundTripCase> VALID_PARSE_CASES = {
    { "main:com.example.App/1.0.0.0/x86_64", "main:com.example.App/1.0.0.0/x86_64" },
    { "stable:com.example.App/1.0.0.0/arm64", "stable:com.example.App/1.0.0.0/arm64" },
    { "com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
    { "unknown:com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
    { "com.example.App/unknown/x86_64", "unknown:com.example.App/unknown/x86_64" },
    { "com.example.App/1.0.0.0/unknown", "unknown:com.example.App/1.0.0.0/unknown" },
    { "com.example.App/1.0.0.0", "unknown:com.example.App/1.0.0.0/unknown" },
    { "com.example.App", "unknown:com.example.App/unknown/unknown" },
    { "main:com.example.App", "main:com.example.App/unknown/unknown" },
    { "main:com.example.App/1.0.0.0", "main:com.example.App/1.0.0.0/unknown" },
    { "main:3333/1.0.0.0/loongarch64", "main:3333/1.0.0.0/loongarch64" },
    { ":com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
};

const std::vector<std::string> EMPTY_ID_INPUTS = {
    "", ":", "main:", "main:/", "/",
};

const std::vector<std::string> MALFORMED_INPUTS = {
    "main:com.example.App/1.0.0.0/x86_64/",
    "main:com.example.App/1.0.0.0/x86_64/extra",
};

} // namespace

TEST(FuzzyReference, ParseRoundTripsToString)
{
    for (const auto &tc : VALID_PARSE_CASES) {
        SCOPED_TRACE(tc.raw);
        auto fuzzy = FuzzyReference::parse(tc.raw);
        ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
        EXPECT_EQ(fuzzy->toString(), tc.expected);
    }
}

TEST(FuzzyReference, ParseExtractsAllFields)
{
    auto fuzzy = FuzzyReference::parse("main:com.example.App/1.0.0.0/x86_64");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_EQ(fuzzy->channel.value_or(""), "main");
    EXPECT_EQ(fuzzy->id, "com.example.App");
    EXPECT_EQ(fuzzy->version.value_or(""), "1.0.0.0");
    ASSERT_TRUE(fuzzy->arch.has_value());
    EXPECT_EQ(fuzzy->arch->toString(), "x86_64");
}

TEST(FuzzyReference, ParseKeepsChannelOptional)
{
    auto fuzzy = FuzzyReference::parse("com.example.App/1.0.0.0/x86_64");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->channel.has_value());
}

TEST(FuzzyReference, ParseNormalizesUnknownChannel)
{
    auto fuzzy = FuzzyReference::parse("unknown:com.example.App/1.0.0.0/x86_64");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->channel.has_value());
    EXPECT_EQ(fuzzy->toString(), "unknown:com.example.App/1.0.0.0/x86_64");
}

TEST(FuzzyReference, ParseKeepsVersionOptional)
{
    auto fuzzy = FuzzyReference::parse("com.example.App");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->version.has_value());
}

TEST(FuzzyReference, ParseNormalizesUnknownVersion)
{
    auto fuzzy = FuzzyReference::parse("com.example.App/unknown/x86_64");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->version.has_value());
    EXPECT_EQ(fuzzy->toString(), "unknown:com.example.App/unknown/x86_64");
}

TEST(FuzzyReference, ParseKeepsArchitectureOptional)
{
    auto fuzzy = FuzzyReference::parse("com.example.App/1.0.0.0");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->arch.has_value());
}

TEST(FuzzyReference, ParseNormalizesUnknownArchitecture)
{
    auto fuzzy = FuzzyReference::parse("com.example.App/1.0.0.0/unknown");
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->arch.has_value());
    EXPECT_EQ(fuzzy->toString(), "unknown:com.example.App/1.0.0.0/unknown");
}

TEST(FuzzyReference, ParseRejectsEmptyId)
{
    for (const auto &raw : EMPTY_ID_INPUTS) {
        SCOPED_TRACE(raw);
        auto fuzzy = FuzzyReference::parse(raw);
        ASSERT_FALSE(fuzzy.has_value()) << raw;
        EXPECT_NE(fuzzy.error().message().find("empty id"), std::string::npos);
    }
}

TEST(FuzzyReference, ParseRejectsMalformedInput)
{
    for (const auto &raw : MALFORMED_INPUTS) {
        SCOPED_TRACE(raw);
        auto fuzzy = FuzzyReference::parse(raw);
        ASSERT_FALSE(fuzzy.has_value()) << raw;
        EXPECT_EQ(fuzzy.error().code(), static_cast<int>(ErrorCode::InvalidFuzzyReference));
    }
}

TEST(FuzzyReference, ParseRejectsUnknownArchitecture)
{
    auto fuzzy = FuzzyReference::parse("main:com.example.App/1.0.0.0/not-an-arch");
    ASSERT_FALSE(fuzzy.has_value());
    EXPECT_EQ(fuzzy.error().code(), static_cast<int>(ErrorCode::UnknownArchitecture));
    EXPECT_NE(fuzzy.error().message().find("unknown architecture"), std::string::npos);
}

TEST(FuzzyReferenceCreate, AcceptsMinimalFields)
{
    auto fuzzy =
      FuzzyReference::create(std::nullopt, "com.example.App", std::nullopt, std::nullopt);
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    EXPECT_FALSE(fuzzy->channel.has_value());
    EXPECT_EQ(fuzzy->id, "com.example.App");
    EXPECT_FALSE(fuzzy->version.has_value());
    EXPECT_FALSE(fuzzy->arch.has_value());
    EXPECT_EQ(fuzzy->toString(), "unknown:com.example.App/unknown/unknown");
}

TEST(FuzzyReferenceCreate, AcceptsAllFields)
{
    auto fuzzy = FuzzyReference::create("main",
                                        "com.example.App",
                                        "1.0.0.0",
                                        Architecture(Architecture::X86_64));
    ASSERT_TRUE(fuzzy.has_value()) << fuzzy.error().message();
    ASSERT_TRUE(fuzzy->channel.has_value());
    EXPECT_EQ(*fuzzy->channel, "main");
    EXPECT_EQ(fuzzy->id, "com.example.App");
    ASSERT_TRUE(fuzzy->version.has_value());
    EXPECT_EQ(*fuzzy->version, "1.0.0.0");
    ASSERT_TRUE(fuzzy->arch.has_value());
    EXPECT_EQ(fuzzy->toString(), "main:com.example.App/1.0.0.0/x86_64");
}

TEST(FuzzyReferenceCreate, RejectsEmptyChannel)
{
    auto fuzzy = FuzzyReference::create("", "com.example.App", std::nullopt, std::nullopt);
    ASSERT_FALSE(fuzzy.has_value());
    EXPECT_NE(fuzzy.error().message().find("empty channel"), std::string::npos);
}

TEST(FuzzyReferenceCreate, RejectsEmptyId)
{
    auto fuzzy = FuzzyReference::create("main", "", std::nullopt, std::nullopt);
    ASSERT_FALSE(fuzzy.has_value());
    EXPECT_NE(fuzzy.error().message().find("empty id"), std::string::npos);
}

TEST(FuzzyReferenceCreate, RejectsEmptyChannelAndId)
{
    auto fuzzy = FuzzyReference::create("", "", std::nullopt, std::nullopt);
    ASSERT_FALSE(fuzzy.has_value());
    EXPECT_NE(fuzzy.error().message().find("empty channel"), std::string::npos);
}
