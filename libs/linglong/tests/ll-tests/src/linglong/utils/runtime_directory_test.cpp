// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/env.h"
#include "linglong/utils/runtime_directory.h"

#include <common/tempdir.h>

#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace linglong::utils::runtime_directory;

namespace {

void setPermissions(const std::filesystem::path &path, std::filesystem::perms permissions)
{
    std::error_code ec;
    std::filesystem::permissions(path, permissions, std::filesystem::perm_options::replace, ec);
    ASSERT_FALSE(ec) << ec.message();
}

mode_t modeOf(const std::filesystem::path &path)
{
    struct stat st{};
    EXPECT_EQ(::lstat(path.c_str(), &st), 0);
    return st.st_mode & 07777;
}

std::filesystem::path makeSafeParent(const std::filesystem::path &base)
{
    auto parent = base / "user";
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::create_directories(parent, ec));
    EXPECT_FALSE(ec) << ec.message();
    setPermissions(parent,
                   std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                     | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                     | std::filesystem::perms::others_exec);
    return parent;
}

} // namespace

TEST(RuntimeDirectoryTest, PrepareAppRuntimeRejectsSymlinkDirectories)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    auto parent = makeSafeParent(sandbox.path());
    auto target = sandbox.path() / "target";
    ASSERT_TRUE(std::filesystem::create_directory(target));

    std::error_code ec;
    auto runtimeLink = parent / "runtime-link";
    std::filesystem::create_directory_symlink(target, runtimeLink, ec);
    ASSERT_FALSE(ec);
    EXPECT_FALSE(preparePrivilegedAppRuntimeDir(runtimeLink, "org.deepin.demo", ::geteuid()));
    std::filesystem::remove(runtimeLink, ec);

    auto parentLink = sandbox.path() / "parent-link";
    std::filesystem::create_directory_symlink(parent, parentLink, ec);
    ASSERT_FALSE(ec);
    EXPECT_FALSE(preparePrivilegedAppRuntimeDir(parentLink / "0", "org.deepin.demo", ::geteuid()));
    std::filesystem::remove(parentLink, ec);
}

TEST(RuntimeDirectoryTest, PrepareAppRuntimeRejectsUnsafeParent)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    auto parent = makeSafeParent(sandbox.path());
    auto runtimeDir = parent / "0";
    const auto owner = ::geteuid();

    EXPECT_FALSE(preparePrivilegedAppRuntimeDir(runtimeDir, "org.deepin.demo", owner == 0 ? 1 : 0));

    std::error_code ec;
    std::filesystem::permissions(parent,
                                 std::filesystem::perms::group_write,
                                 std::filesystem::perm_options::add,
                                 ec);
    ASSERT_FALSE(ec);
    EXPECT_FALSE(preparePrivilegedAppRuntimeDir(runtimeDir, "org.deepin.demo", owner));
}

TEST(RuntimeDirectoryTest, RejectsInvalidAppIds)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    const auto owner = ::geteuid();

    const std::vector<std::string> invalidAppIds{
        "", ".", "..", "../escape", "foo/bar", "/absolute", "foo\\bar", std::string("foo\0bar", 7),
    };

    for (const auto &appId : invalidAppIds) {
        auto privilegedRuntimeDir = sandbox.path() / "privileged";
        auto privilegedResult = preparePrivilegedAppRuntimeDir(privilegedRuntimeDir, appId, owner);
        EXPECT_FALSE(privilegedResult);
        EXPECT_FALSE(std::filesystem::exists(privilegedRuntimeDir));
    }
}

TEST(RuntimeDirectoryTest, PrepareAppRuntimeCreatesPrivateTree)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    auto parent = makeSafeParent(sandbox.path());
    auto runtimeDir = parent / "0";
    const auto owner = ::geteuid();

    auto result = preparePrivilegedAppRuntimeDir(runtimeDir, "org.deepin.demo", owner);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(*result, runtimeDir / "linglong" / "apps" / "org.deepin.demo");
    EXPECT_EQ(modeOf(runtimeDir), S_IRWXU);
    EXPECT_EQ(modeOf(*result), S_IRWXU);
}

TEST(RuntimeDirectoryTest, PrepareAppRuntimeRejectsExistingNonPrivateRuntime)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    auto parent = makeSafeParent(sandbox.path());
    auto runtimeDir = parent / "0";
    ASSERT_TRUE(std::filesystem::create_directory(runtimeDir));
    setPermissions(runtimeDir,
                   std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                     | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                     | std::filesystem::perms::others_exec);

    auto result = preparePrivilegedAppRuntimeDir(runtimeDir, "org.deepin.demo", ::geteuid());
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("0700"));
}

TEST(RuntimeDirectoryTest, PrepareAppRuntimeTightensLinglongSubtree)
{
    TempDir sandbox{ "ll-runtime-dir-" };
    auto parent = makeSafeParent(sandbox.path());
    auto runtimeDir = parent / "0";
    auto appsDir = runtimeDir / "linglong" / "apps";
    ASSERT_TRUE(std::filesystem::create_directories(appsDir));
    setPermissions(runtimeDir, std::filesystem::perms::owner_all);
    setPermissions(runtimeDir / "linglong",
                   std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                     | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                     | std::filesystem::perms::others_exec);
    setPermissions(appsDir,
                   std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                     | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                     | std::filesystem::perms::others_exec);

    auto result = preparePrivilegedAppRuntimeDir(runtimeDir, "org.deepin.demo", ::geteuid());
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(modeOf(runtimeDir / "linglong"), S_IRWXU);
    EXPECT_EQ(modeOf(appsDir), S_IRWXU);
}

TEST(RuntimeDirectoryTest, UserLayoutPreservesExistingRuntimePaths)
{
    TempDir sandbox{ "ll-runtime-layout-" };
    const auto xdgRuntimeDir = sandbox.path() / "xdg";
    const auto layout = RuntimeLayout::forUser(xdgRuntimeDir);

    EXPECT_FALSE(layout.isPrivilegedHost());
    EXPECT_EQ(layout.root(), xdgRuntimeDir);
    EXPECT_EQ(layout.runtimeDir(), xdgRuntimeDir / "linglong");
    EXPECT_EQ(layout.bundleDir("container-id"), xdgRuntimeDir / "linglong" / "container-id");
    EXPECT_EQ(layout.bundleDir("container-id", ".init"),
              xdgRuntimeDir / "linglong" / "container-id.init");
    EXPECT_EQ(layout.appRuntimeDir("org.deepin.demo"),
              xdgRuntimeDir / "linglong" / "apps" / "org.deepin.demo");
    EXPECT_EQ(layout.containerStateDir(), xdgRuntimeDir / "linglong" / "box");
    EXPECT_EQ(layout.consoleSocket("socket-abcdef"), xdgRuntimeDir / "linglong" / "socket-abcdef");
}

TEST(RuntimeDirectoryTest, PrivilegedLayoutUsesOnePrivateRoot)
{
    TempDir sandbox{ "ll-runtime-layout-" };
    auto parent = makeSafeParent(sandbox.path());
    const auto runtimeRoot = parent / "0";
    const auto layout = RuntimeLayout::forPrivilegedHost(runtimeRoot);

    EXPECT_TRUE(layout.isPrivilegedHost());
    EXPECT_EQ(layout.bundleDir("container-id"), runtimeRoot / "linglong" / "container-id");
    EXPECT_EQ(layout.appRuntimeDir("org.deepin.demo"),
              runtimeRoot / "linglong" / "apps" / "org.deepin.demo");
    EXPECT_EQ(layout.containerStateDir(), runtimeRoot / "linglong" / "box");
    EXPECT_EQ(layout.consoleSocket("socket-abcdef"), runtimeRoot / "linglong" / "socket-abcdef");

    auto result = layout.prepareAppRuntimeDir("org.deepin.demo");
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(*result, runtimeRoot / "linglong" / "apps" / "org.deepin.demo");
    EXPECT_EQ(modeOf(runtimeRoot), S_IRWXU);
    EXPECT_EQ(modeOf(*result), S_IRWXU);
}

TEST(RuntimeDirectoryTest, CurrentNonPrivilegedLayoutUsesXDGRuntimeDir)
{
    if (::geteuid() == 0) {
        GTEST_SKIP() << "test assumes a non-root test process";
    }

    TempDir sandbox{ "ll-runtime-layout-" };
    const auto xdgRuntimeDir = sandbox.path() / "xdg";
    linglong::utils::EnvironmentVariableGuard guard("XDG_RUNTIME_DIR", xdgRuntimeDir.string());

    auto layout = RuntimeLayout::current();
    ASSERT_TRUE(layout) << layout.error().message();
    EXPECT_FALSE(layout->isPrivilegedHost());
    EXPECT_EQ(layout->root(), xdgRuntimeDir);
}

TEST(RuntimeDirectoryTest, RootInUserNamespaceUsesXDGRuntimeDir)
{
    if (::geteuid() != 0 || ::getenv("LINGLONG_TEST_IN_USERNS") == nullptr) {
        GTEST_SKIP() << "test requires root credentials inside a user namespace";
    }

    TempDir sandbox{ "ll-runtime-layout-" };
    const auto xdgRuntimeDir = sandbox.path() / "xdg";
    linglong::utils::EnvironmentVariableGuard guard("XDG_RUNTIME_DIR", xdgRuntimeDir.string());

    auto layout = RuntimeLayout::current();
    ASSERT_TRUE(layout) << layout.error().message();
    EXPECT_FALSE(layout->isPrivilegedHost());
    EXPECT_EQ(layout->root(), xdgRuntimeDir);
}
