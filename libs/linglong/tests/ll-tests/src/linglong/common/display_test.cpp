// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/display.h"

namespace linglong::common::display {

TEST(DisplayTest, GetXOrgDisplay_SimpleHostDisplay)
{
    auto result = getXOrgDisplay("localhost:0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host.value(), "localhost");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
    EXPECT_FALSE(result->protocol.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_HostNameDisplayScreen)
{
    auto result = getXOrgDisplay("example.com:10.2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host.value(), "example.com");
    EXPECT_EQ(result->displayNo, 10);
    EXPECT_EQ(result->screenNo, 2);
    EXPECT_FALSE(result->protocol.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_WithProtocol)
{
    auto result = getXOrgDisplay("tcp/localhost:0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->protocol.value(), "tcp");
    EXPECT_EQ(result->host.value(), "localhost");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_WithProtocolAndHost)
{
    auto result = getXOrgDisplay("tcp/example.com:1.0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->protocol.value(), "tcp");
    EXPECT_EQ(result->host.value(), "example.com");
    EXPECT_EQ(result->displayNo, 1);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_UnixPrefixWithHost)
{
    auto result = getXOrgDisplay("unix:/tmp/.X11-unix/X0");
    if (!result) {
        GTEST_SKIP();
    }

    EXPECT_EQ(result->host.value(), "/tmp/.X11-unix/X0");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
    EXPECT_EQ(result->protocol, "unix");
}

TEST(DisplayTest, GetXOrgDisplay_AbsolutePath)
{
    auto result = getXOrgDisplay("/tmp/.X11-unix/X0");
    if (!result) {
        GTEST_SKIP();
    }

    EXPECT_EQ(result->host.value(), "/tmp/.X11-unix/X0");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
    EXPECT_EQ(result->protocol, "unix");
}

TEST(DisplayTest, GetXOrgDisplay_InvalidNoColon)
{
    auto result = getXOrgDisplay("invalid_display");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_InvalidDisplayNo)
{
    auto result = getXOrgDisplay("localhost:abc");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_InvalidScreenNo)
{
    auto result = getXOrgDisplay("localhost:0.abc");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_ComplexProtocol)
{
    auto result = getXOrgDisplay("vnc/user@host:5.1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->protocol.value(), "vnc");
    EXPECT_EQ(result->host.value(), "user@host");
    EXPECT_EQ(result->displayNo, 5);
    EXPECT_EQ(result->screenNo, 1);
}

TEST(DisplayTest, GetXOrgDisplay_MissingHost)
{
    auto result = getXOrgDisplay(":0");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->host.has_value());
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_ZeroValues)
{
    auto result = getXOrgDisplay(":0.0");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->host.has_value());
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_MinimalDisplay)
{
    auto result = getXOrgDisplay(":1");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->host.has_value());
    EXPECT_EQ(result->displayNo, 1);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_IPv6Address)
{
    auto result = getXOrgDisplay("[2001:db8::1]:0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host.value(), "[2001:db8::1]");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 0);
}

TEST(DisplayTest, GetXOrgDisplay_IPv6AddressWithScreen)
{
    auto result = getXOrgDisplay("[::ffff:192.0.2.1]:0.1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host.value(), "[::ffff:192.0.2.1]");
    EXPECT_EQ(result->displayNo, 0);
    EXPECT_EQ(result->screenNo, 1);
}

TEST(DisplayTest, GetXOrgDisplay_MultipleColonsInHost)
{
    auto result = getXOrgDisplay("proto/host:name:999.1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->protocol.value(), "proto");
    EXPECT_EQ(result->host.value(), "host:name");
    EXPECT_EQ(result->displayNo, 999);
    EXPECT_EQ(result->screenNo, 1);
}

TEST(DisplayTest, GetXOrgDisplay_NegativeDisplayNo)
{
    auto result = getXOrgDisplay("example.com:-1");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_NegativeScreenNo)
{
    auto result = getXOrgDisplay("example.com:0.-1");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_DisplayNoOverflow)
{
    auto result = getXOrgDisplay("example.com:999999999999999999");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_ScreenNoOverflow)
{
    auto result = getXOrgDisplay("example.com:0.999999999999999999");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_EmptyString)
{
    auto result = getXOrgDisplay("");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_OnlyColon)
{
    auto result = getXOrgDisplay(":");
    ASSERT_FALSE(result.has_value());
}

TEST(DisplayTest, GetXOrgDisplay_NoDisplayNo)
{
    auto result = getXOrgDisplay("test:");
    ASSERT_FALSE(result.has_value());
}

} // namespace linglong::common::display
