
// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/namespace.h"

TEST(NamespaceTest, ParseSubuidRanges)
{
    uid_t uid = 0;
    // multiple ranges for the same user
    std::string content1 = "user1:100000:65536\n"
                           "user2:200000:65536\n"
                           "user1:300000:1000\n";
    std::stringstream ss1(content1);
    auto ranges1 = linglong::utils::detail::parseSubuidRanges(ss1, uid, "user1");
    ASSERT_TRUE(ranges1.has_value());
    ASSERT_EQ(ranges1->size(), 2);
    EXPECT_EQ((*ranges1)[0].subuid, "100000");
    EXPECT_EQ((*ranges1)[0].count, "65536");
    EXPECT_EQ((*ranges1)[1].subuid, "300000");
    EXPECT_EQ((*ranges1)[1].count, "1000");

    // user not exists
    std::stringstream ss2(content1);
    auto ranges2 = linglong::utils::detail::parseSubuidRanges(ss2, uid, "nonexistent");
    ASSERT_TRUE(ranges2.has_value());
    EXPECT_TRUE((*ranges2).empty());

    // missing field
    std::string content3 = "user1:100000\n";
    std::stringstream ss3(content3);
    auto ranges3 = linglong::utils::detail::parseSubuidRanges(ss3, uid, "user1");
    ASSERT_FALSE(ranges3.has_value());

    // empty input
    std::stringstream ss4("");
    auto ranges4 = linglong::utils::detail::parseSubuidRanges(ss4, uid, "user1");
    ASSERT_TRUE(ranges4.has_value());
    EXPECT_TRUE((*ranges4).empty());

    // username is prefix of another username
    std::string content5 = "user1_long:100000:65536\n"
                           "user1:200000:65536\n";
    std::stringstream ss5(content5);
    auto ranges5 = linglong::utils::detail::parseSubuidRanges(ss5, uid, "user1");
    ASSERT_TRUE(ranges5.has_value());
    ASSERT_EQ(ranges5->size(), 1);
    EXPECT_EQ((*ranges5)[0].subuid, "200000");
    EXPECT_EQ((*ranges5)[0].count, "65536");

    // use uid
    std::string content6 = "1000:100000:65536\n"
                           "user1:200000:65536\n";
    std::stringstream ss6(content6);
    auto ranges6 = linglong::utils::detail::parseSubuidRanges(ss6, 1000, "user1");
    ASSERT_TRUE(ranges6.has_value());
    ASSERT_EQ(ranges6->size(), 2);
    EXPECT_EQ((*ranges6)[0].subuid, "100000");
    EXPECT_EQ((*ranges6)[0].count, "65536");
    EXPECT_EQ((*ranges6)[1].subuid, "200000");
    EXPECT_EQ((*ranges6)[1].count, "65536");
}