// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/cmd.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {

void writeFileWithMode(const std::filesystem::path &path,
                       const std::string &content,
                       std::filesystem::perms mode)
{
    std::ofstream out(path);
    out << content;
    out.close();
    std::filesystem::permissions(path, mode);
}

class PathEnvGuard
{
public:
    PathEnvGuard()
        : hadPath(::getenv("PATH") != nullptr)
    {
        if (const char *path = ::getenv("PATH"); path != nullptr) {
            savedPath = path;
        }
    }

    ~PathEnvGuard()
    {
        if (hadPath) {
            ::setenv("PATH", savedPath.c_str(), 1);
        } else {
            ::unsetenv("PATH");
        }
    }

    PathEnvGuard(const PathEnvGuard &) = delete;
    PathEnvGuard &operator=(const PathEnvGuard &) = delete;

private:
    std::string savedPath;
    bool hadPath;
};

TEST(command, Exec)
{
    auto ret = linglong::utils::Cmd("echo").exec({ "-n", "hello" });
    EXPECT_TRUE(ret);
    EXPECT_EQ(ret->size(), 5);
    EXPECT_EQ(*ret, "hello");
    auto userId = linglong::utils::Cmd("id").exec({ "-u" });
    EXPECT_TRUE(userId.has_value());

    userId = userId->substr(0, userId->find('\n'));
    EXPECT_EQ(*userId, std::to_string(getuid()));

    // 测试command不存在时
    auto ret3 = linglong::utils::Cmd("nonexistent").exec();
    EXPECT_FALSE(ret3.has_value());

    // 测试exec出错时
    auto ret4 = linglong::utils::Cmd("ls").exec({ "/linglong/nonexistent" });
    EXPECT_FALSE(ret4.has_value());
}

TEST(command, setEnv)
{
    linglong::utils::Cmd cmd("bash");
    // test set
    cmd.setEnv("LINGLONG_TEST_SETENV", "OK");
    auto existsRef = cmd.exists();
    EXPECT_TRUE(existsRef);
    // test unset
    cmd.setEnv("PATH", "");
    auto ret = cmd.exec({ "-c", "export" });
    EXPECT_TRUE(ret.has_value()) << "failed to execute export command: " << ret.error().message();
    EXPECT_TRUE(ret->find("declare -x LINGLONG_TEST_SETENV=") != std::string::npos) << *ret;
    EXPECT_FALSE(ret->find("declare -x PATH=\"") != std::string::npos) << *ret;
}

TEST(command, toStdin)
{
    // Test writing to stdin using cat command
    auto ret = linglong::utils::Cmd("cat").toStdin("Hello, World!").exec();

    EXPECT_TRUE(ret.has_value()) << "failed to execute cat command: " << ret.error().message();
    EXPECT_EQ(*ret, "Hello, World!");

    // Test with multiline input
    auto ret2 = linglong::utils::Cmd("wc").toStdin("line1\nline2\nline3\n").exec({ "-l" });

    EXPECT_TRUE(ret2.has_value()) << "failed to execute wc command: " << ret2.error().message();
    // wc -l should output " 3" for 3 lines
    EXPECT_TRUE(ret2->find("3") != std::string::npos) << *ret2;

    // Test empty stdin
    auto ret3 = linglong::utils::Cmd("cat").toStdin("").exec();

    EXPECT_TRUE(ret3.has_value()) << "failed to execute cat with empty stdin: "
                                  << ret3.error().message();
    EXPECT_TRUE(ret3->empty()) << "cat with empty stdin should return empty output";

    // Test with data exceeding 1M
    std::string large_data(1048576 + 1024, 'A');
    auto ret4 = linglong::utils::Cmd("wc").toStdin(large_data).exec({ "-c" });

    EXPECT_TRUE(ret4.has_value()) << "failed to execute wc with large stdin: "
                                  << ret4.error().message();
    EXPECT_TRUE(ret4->find("1049600") != std::string::npos)
      << "wc -c should report 1049600 bytes, got: " << *ret4;
}

// Absolute paths and PATH lookup must only accept executable regular files.
TEST(command, RequiresExecutableRegularFile)
{
    TempDir temp_dir;

    auto dir = temp_dir.path() / "a-directory";
    std::filesystem::create_directory(dir);
    EXPECT_FALSE(linglong::utils::Cmd(dir.string()).exists());

    auto plain = temp_dir.path() / "plain.txt";
    writeFileWithMode(plain,
                      "not a command\n",
                      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
                        | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
    EXPECT_FALSE(linglong::utils::Cmd(plain.string()).exists());
    auto plainExec = linglong::utils::Cmd(plain.string()).exec();
    EXPECT_FALSE(plainExec.has_value());

    auto script = temp_dir.path() / "ok-script";
    writeFileWithMode(script,
                      "#!/bin/sh\nexit 0\n",
                      std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                        | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                        | std::filesystem::perms::others_exec);
    EXPECT_TRUE(linglong::utils::Cmd(script.string()).exists());
    auto scriptExec = linglong::utils::Cmd(script.string()).exec();
    EXPECT_TRUE(scriptExec.has_value()) << scriptExec.error().message();
}

TEST(command, PathLookupSkipsNonExecutableCandidates)
{
    PathEnvGuard pathGuard;
    TempDir dir_with_directory;
    TempDir dir_with_plain;
    TempDir dir_with_exec;

    // Unique name so BINDIR cannot satisfy the lookup.
    const std::string name = "linglong-cmd-path-probe";
    std::filesystem::create_directory(dir_with_directory.path() / name);
    writeFileWithMode(dir_with_plain.path() / name,
                      "not executable\n",
                      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    writeFileWithMode(dir_with_exec.path() / name,
                      "#!/bin/sh\nprintf path-ok\n",
                      std::filesystem::perms::owner_all | std::filesystem::perms::group_read
                        | std::filesystem::perms::group_exec | std::filesystem::perms::others_read
                        | std::filesystem::perms::others_exec);

    // Directory and non-executable regular file must not be treated as commands.
    ::setenv("PATH",
             (dir_with_directory.path().string() + ":" + dir_with_plain.path().string()).c_str(),
             1);
    EXPECT_FALSE(linglong::utils::Cmd(name).exists());

    // An executable regular file later in PATH is accepted.
    ::setenv("PATH",
             (dir_with_directory.path().string() + ":" + dir_with_plain.path().string() + ":"
              + dir_with_exec.path().string())
               .c_str(),
             1);
    EXPECT_TRUE(linglong::utils::Cmd(name).exists());
    auto ret = linglong::utils::Cmd(name).exec();
    EXPECT_TRUE(ret.has_value()) << ret.error().message();
    EXPECT_EQ(*ret, "path-ok");
}

} // namespace
