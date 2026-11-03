// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/common/display.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/temporary_directory.h"

#include <fstream>

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

TEST(DisplayTest, GetWaylandDisplay_AbsoluteExistingPath)
{
    linglong::utils::TemporaryDirectory tempDir =
      linglong::utils::TemporaryDirectory::create("display-test").value();
    auto socketPath = tempDir.path() / "wayland-0";
    std::ofstream(socketPath).close();
    ASSERT_TRUE(std::filesystem::exists(socketPath));

    auto result = getWaylandDisplay(socketPath.string());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), socketPath);
}

TEST(DisplayTest, GetWaylandDisplay_AbsoluteMissingPath)
{
    auto missing = std::filesystem::temp_directory_path() / "no-such-wayland-socket-xyz";
    auto result = getWaylandDisplay(missing.string());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error(), ::testing::HasSubstr("not found"));
}

TEST(DisplayTest, GetWaylandDisplay_RelativePathResolvedUnderXdgRuntimeDir)
{
    linglong::utils::TemporaryDirectory tempDir =
      linglong::utils::TemporaryDirectory::create("display-test").value();
    auto old = ::getenv("XDG_RUNTIME_DIR");
    auto rdir = tempDir.path();
    ::setenv("XDG_RUNTIME_DIR", rdir.string().c_str(), 1);
    auto restore = linglong::utils::finally::finally([old]() {
        if (old) {
            ::setenv("XDG_RUNTIME_DIR", old, 1);
        } else {
            ::unsetenv("XDG_RUNTIME_DIR");
        }
    });

    auto socketPath = rdir / "wayland-1";
    std::ofstream(socketPath).close();
    ASSERT_TRUE(std::filesystem::exists(socketPath));

    auto result = getWaylandDisplay("wayland-1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), rdir / "wayland-1");
}

TEST(DisplayTest, GetXOrgAuthFile_ProvidedExistingFile)
{
    linglong::utils::TemporaryDirectory tempDir =
      linglong::utils::TemporaryDirectory::create("display-test").value();
    auto authFile = tempDir.path() / "Xauthority";
    std::ofstream(authFile).close();

    auto result = getXOrgAuthFile(authFile.string());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), authFile);
}

TEST(DisplayTest, GetXOrgAuthFile_ProvidedMissingFile)
{
    auto missing = std::filesystem::temp_directory_path() / "no-such-xauthority-xyz";
    auto result = getXOrgAuthFile(missing.string());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error(), ::testing::HasSubstr("not found"));
}

TEST(DisplayTest, GetXOrgAuthFile_EmptyFallsBackToHomeWhenFileExists)
{
    auto homeDir = std::filesystem::temp_directory_path() / "display-test-home-xyz";
    std::filesystem::create_directories(homeDir);
    auto authFile = homeDir / ".Xauthority";
    std::ofstream(authFile).close();

    auto old = ::getenv("HOME");
    ::setenv("HOME", homeDir.string().c_str(), 1);
    auto restore = linglong::utils::finally::finally([old]() {
        if (old) {
            ::setenv("HOME", old, 1);
        } else {
            ::unsetenv("HOME");
        }
    });

    auto result = getXOrgAuthFile("");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), authFile);
}

} // namespace linglong::common::display
