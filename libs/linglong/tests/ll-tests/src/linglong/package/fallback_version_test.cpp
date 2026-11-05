/*
 ; SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <linglong/package/fallback_version.h>

using namespace linglong::package;

TEST(FallbackVersionTest, parse)
{
    auto result = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");

    result = FallbackVersion::parse("1.2.a");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "a");

    result = FallbackVersion::parse("1.2.3_alpha");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3_alpha");

    result = FallbackVersion::parse("1.2.3.4.5.6.7");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 7);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");
    EXPECT_EQ(result->list[3], "4");
    EXPECT_EQ(result->list[4], "5");
    EXPECT_EQ(result->list[5], "6");
    EXPECT_EQ(result->list[6], "7");

    result = FallbackVersion::parse("1.2.3@#$%^&*()");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3@#$%^&*()");

    result = FallbackVersion::parse("1.2.3 alpha");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3 alpha");

    result = FallbackVersion::parse("1.2.3-测试");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-测试");

    result = FallbackVersion::parse("1.2.3-😊");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-😊");

    EXPECT_FALSE(FallbackVersion::parse("").has_value());
    EXPECT_FALSE(FallbackVersion::parse("...").has_value());

    result = FallbackVersion::parse("1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 20);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");
    EXPECT_EQ(result->list[3], "4");
    EXPECT_EQ(result->list[4], "5");
    EXPECT_EQ(result->list[5], "6");
    EXPECT_EQ(result->list[6], "7");
    EXPECT_EQ(result->list[7], "8");
    EXPECT_EQ(result->list[8], "9");
    EXPECT_EQ(result->list[9], "10");
    EXPECT_EQ(result->list[10], "11");
    EXPECT_EQ(result->list[11], "12");
    EXPECT_EQ(result->list[12], "13");
    EXPECT_EQ(result->list[13], "14");
    EXPECT_EQ(result->list[14], "15");
    EXPECT_EQ(result->list[15], "16");
    EXPECT_EQ(result->list[16], "17");
    EXPECT_EQ(result->list[17], "18");
    EXPECT_EQ(result->list[18], "19");
    EXPECT_EQ(result->list[19], "20");

    result = FallbackVersion::parse("1.2.3\n4\t5");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3\n4\t5");

    result = FallbackVersion::parse("1.2.3-日本語");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-日本語");
}

// Fallback version comparison
TEST(FallbackVersionTest, compare)
{
    // With letters
    EXPECT_EQ(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.a").value());
    EXPECT_NE(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.b").value());
    EXPECT_LT(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.b").value());
    EXPECT_GT(FallbackVersion::parse("1.2.b").value(), FallbackVersion::parse("1.2.a").value());
    // With special characters
    EXPECT_EQ(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_alpha").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_beta").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_beta").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3_beta").value(),
              FallbackVersion::parse("1.2.3_alpha").value());
    // Long version
    EXPECT_EQ(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.7").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.8").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.8").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3.4.5.6.8").value(),
              FallbackVersion::parse("1.2.3.4.5.6.7").value());

    // With various special characters
    EXPECT_EQ(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()_+").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()_+").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3@#$%^&*()_+").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()").value());

    // With spaces
    EXPECT_EQ(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 alpha").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 beta").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 beta").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3 beta").value(),
              FallbackVersion::parse("1.2.3 alpha").value());

    // With Chinese characters
    EXPECT_EQ(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-测试").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-版本").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-版本").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3-版本").value(),
              FallbackVersion::parse("1.2.3-测试").value());

    // With emoji
    EXPECT_EQ(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-😊").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-🚀").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-🚀").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3-🚀").value(),
              FallbackVersion::parse("1.2.3-😊").value());

    // Leading-zero variants are distinct; order them by their raw spelling
    EXPECT_NE(FallbackVersion::parse("1.02.0003").value(), FallbackVersion::parse("1.2.3").value());
    EXPECT_LT(FallbackVersion::parse("1.02.0003").value(), FallbackVersion::parse("1.2.3").value());
    EXPECT_LT(FallbackVersion::parse("3").value(), FallbackVersion::parse("2147483648").value());
    EXPECT_LT(FallbackVersion::parse("18446744073709551616").value(),
              FallbackVersion::parse("18446744073709551617").value());

    // Large numbers with different digit counts (string comparison gets this wrong)
    EXPECT_LT(FallbackVersion::parse("1.9999999999999").value(),
              FallbackVersion::parse("1.10000000000000").value());
    EXPECT_LT(FallbackVersion::parse("9").value(), FallbackVersion::parse("10000000000").value());

    // Leading-zero variants are distinct and ordered deterministically under strict weak order
    const auto zeroPadded = FallbackVersion::parse("1.09.alpha").value();
    const auto canonical = FallbackVersion::parse("1.9.alpha").value();
    const auto other = FallbackVersion::parse("1.8x.alpha").value();
    EXPECT_NE(zeroPadded, canonical);
    EXPECT_LT(zeroPadded, canonical);
    EXPECT_LT(other, zeroPadded);
    EXPECT_GT(zeroPadded, other);
    EXPECT_GT(canonical, other);

    // Parts with numeric prefixes compare numerically
    EXPECT_LT(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.4").value());
    EXPECT_GT(FallbackVersion::parse("1.10_alpha").value(),
              FallbackVersion::parse("1.9_zzz").value());

    // Empty versions and versions with only separators
    EXPECT_FALSE(FallbackVersion::parse("").has_value());
    EXPECT_FALSE(FallbackVersion::parse("...").has_value());
    // Extremely long version
    EXPECT_TRUE(
      FallbackVersion::parse("1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20").has_value());

    // Version with a newline and a tab
    EXPECT_TRUE(FallbackVersion::parse("1.2.3\n4\t5").has_value());

    // Version with Unicode characters
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-日本語").has_value());
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-한국어").has_value());
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-русский").has_value());
}

TEST(FallbackVersionTest, semanticMatch)
{
    EXPECT_TRUE(FallbackVersion::parse("1.2.3").value().semanticMatch("1.2"));
    EXPECT_TRUE(FallbackVersion::parse("1.2.3").value().semanticMatch("1.2.3"));

    // Leading-zero forms stay distinct here, consistent with operator==
    EXPECT_FALSE(FallbackVersion::parse("1.02.0003").value().semanticMatch("1.2.3"));
    EXPECT_FALSE(FallbackVersion::parse("1.02.0003").value().semanticMatch("1.2"));
    EXPECT_TRUE(FallbackVersion::parse("1.02.0003").value().semanticMatch("1.02"));

    EXPECT_FALSE(FallbackVersion::parse("1.2.3").value().semanticMatch("1.3"));
    EXPECT_FALSE(FallbackVersion::parse("1.2.3").value().semanticMatch("1.2.4"));
    EXPECT_FALSE(FallbackVersion::parse("1.2.3").value().semanticMatch("1.2.3.4"));
}
