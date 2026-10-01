// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/runtime/wayland_socket_addr.h"

#include <cstddef>
#include <string>
#include <string_view>

#include <sys/socket.h>
#include <sys/un.h>

namespace {

using linglong::runtime::makeWaylandSocketAddr;
using linglong::runtime::waylandSocketAddrLength;

// Every test is written relative to the real capacity of sun_path instead of a
// hard coded 108, so the tests stay meaningful on platforms where the size of
// the member differs.
constexpr std::size_t sunPathCapacity{ sizeof(sockaddr_un{}.sun_path) };

// Returns a path of exactly `size` characters. The "/tmp/" prefix keeps it
// looking like an absolute path, the padding makes the length deterministic.
std::string pathWithSize(std::size_t size)
{
    constexpr std::string_view prefix{ "/tmp/" };

    if (size <= prefix.size()) {
        return std::string{ prefix.substr(0, size) };
    }

    std::string path{ prefix };
    path.append(size - prefix.size(), 'a');
    return path;
}

// The bundle directory used by `ll-cli run` when XDG_RUNTIME_DIR is not set:
// /tmp/linglong-runtime-<uid>/linglong/<64 characters of the container id>.
std::string fallbackBundleDir()
{
    return "/tmp/linglong-runtime-1000/linglong/" + std::string(64, 'a');
}

TEST(WaylandSocketAddrTest, BuildsAddressForFittingPath)
{
    const std::string path{ "/run/user/1000/linglong/wayland-socket" };

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_TRUE(addr) << addr.error().message();

    const std::string built{ addr->sun_path, path.size() };
    EXPECT_EQ(addr->sun_family, AF_UNIX);
    EXPECT_EQ(built, path);
    EXPECT_EQ(addr->sun_path[path.size()], '\0');

    // Everything after the terminating NUL byte has to stay untouched,
    // otherwise the copy already wrote past the end of sun_path.
    for (std::size_t i = path.size() + 1; i < sunPathCapacity; ++i) {
        EXPECT_EQ(addr->sun_path[i], '\0') << "byte " << i << " was overwritten";
    }

    const auto length = waylandSocketAddrLength(path);
    EXPECT_EQ(length, offsetof(struct sockaddr_un, sun_path) + path.size() + 1);
    EXPECT_LE(length, sizeof(struct sockaddr_un));
}

TEST(WaylandSocketAddrTest, AcceptsLongestPathThatStillFits)
{
    const auto path = pathWithSize(sunPathCapacity - 1);
    ASSERT_EQ(path.size(), sunPathCapacity - 1);

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_TRUE(addr) << addr.error().message();

    const std::string built{ addr->sun_path, path.size() };
    EXPECT_EQ(addr->sun_family, AF_UNIX);
    EXPECT_EQ(built, path);
    EXPECT_EQ(addr->sun_path[sunPathCapacity - 1], '\0');

    // The longest accepted path uses up the whole structure.
    EXPECT_EQ(waylandSocketAddrLength(path),
              offsetof(struct sockaddr_un, sun_path) + sunPathCapacity);
    EXPECT_LE(waylandSocketAddrLength(path), sizeof(struct sockaddr_un));
}

TEST(WaylandSocketAddrTest, AcceptsRealisticBundleSocketPath)
{
    // /run/user/<uid>/linglong/<64 hex container id>/wayland-socket, the
    // layout used when XDG_RUNTIME_DIR points at the default runtime dir.
    const auto path = "/run/user/1000/linglong/" + std::string(64, 'a') + "/wayland-socket";
    ASSERT_LT(path.size(), sunPathCapacity);

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_TRUE(addr) << addr.error().message();

    const std::string built{ addr->sun_path, path.size() };
    EXPECT_EQ(built, path);
    EXPECT_LE(waylandSocketAddrLength(path), sizeof(struct sockaddr_un));
}

TEST(WaylandSocketAddrTest, RejectsPathThatFillsSunPath)
{
    const auto path = pathWithSize(sunPathCapacity);
    ASSERT_EQ(path.size(), sunPathCapacity);

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_FALSE(addr);
    EXPECT_THAT(addr.error().message(), ::testing::HasSubstr("too long"));
    EXPECT_THAT(addr.error().message(), ::testing::HasSubstr(path));
}

TEST(WaylandSocketAddrTest, RejectsOverlongPath)
{
    const auto path = pathWithSize(sunPathCapacity + 1);
    ASSERT_EQ(path.size(), sunPathCapacity + 1);

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_FALSE(addr);
    EXPECT_THAT(addr.error().message(), ::testing::HasSubstr("too long"));
}

TEST(WaylandSocketAddrTest, RejectsFallbackBundleSocketPath)
{
    // Regression test for the reported overflow: without XDG_RUNTIME_DIR the
    // bundle directory is /tmp/linglong-runtime-<uid>/linglong/<container id>
    // and the wayland socket below it does not fit into sun_path.
    const auto path = fallbackBundleDir() + "/wayland-socket";
    ASSERT_GT(path.size(), sunPathCapacity);

    auto addr = makeWaylandSocketAddr(path);
    ASSERT_FALSE(addr);
    EXPECT_THAT(addr.error().message(), ::testing::HasSubstr("too long"));
}

TEST(WaylandSocketAddrTest, RejectsEmptyPath)
{
    auto addr = makeWaylandSocketAddr(std::string_view{});
    ASSERT_FALSE(addr);
    EXPECT_THAT(addr.error().message(), ::testing::HasSubstr("empty"));
}

TEST(WaylandSocketAddrTest, RejectionIsRepeatable)
{
    // Building the same invalid address twice must fail in the same way: a
    // previous call must not leave partially filled state behind.
    const auto path = pathWithSize(sunPathCapacity * 2);

    for (auto attempt = 0; attempt < 2; ++attempt) {
        auto addr = makeWaylandSocketAddr(path);
        ASSERT_FALSE(addr) << "attempt " << attempt;
        EXPECT_THAT(addr.error().message(), ::testing::HasSubstr("too long"));
    }
}

} // namespace
