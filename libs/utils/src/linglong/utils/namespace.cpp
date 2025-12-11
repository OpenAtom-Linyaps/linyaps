// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "namespace.h"

#include "linglong/utils/command/cmd.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <sys/capability.h>

#include <fstream>

#include <pwd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {

namespace detail {

utils::error::Result<std::string> getUserName(uid_t uid)
{
    LINGLONG_TRACE("get user name");

    auto bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize < 0) {
        bufSize = 1024;
    }

    std::vector<char> buf(bufSize);
    struct passwd pw;
    struct passwd *result{ nullptr };
    auto res = getpwuid_r(uid, &pw, buf.data(), buf.size(), &result);
    if (res != 0) {
        return LINGLONG_ERR(fmt::format("failed to get user name {}", res).c_str());
    }

    return result->pw_name;
}

utils::error::Result<std::vector<detail::SubuidRange>> parseSubuidRanges(std::istream &stream,
                                                                         uid_t uid,
                                                                         const std::string &name)
{
    LINGLONG_TRACE("parse subuid ranges");

    std::vector<detail::SubuidRange> ranges;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(name + ":", 0) != 0 && line.rfind(std::to_string(uid) + ":", 0) != 0) {
            continue;
        }

        auto pos = line.find(':');
        if (pos != std::string::npos) {
            auto end = line.find(':', pos + 1);
            if (end == std::string::npos) {
                return LINGLONG_ERR("invalid subuid file");
            }
            auto subuid = line.substr(pos + 1, end - pos - 1);
            auto count = line.substr(end + 1);
            ranges.push_back({ subuid, count });
        }
    }

    return ranges;
}

utils::error::Result<std::vector<detail::SubuidRange>> getSubuidRange(uid_t uid, bool isUid)
{
    LINGLONG_TRACE("get subuid range");

    auto username = detail::getUserName(uid);
    if (!username) {
        return LINGLONG_ERR("failed to get user name", username.error());
    }

    const auto *filename = isUid ? "/etc/subuid" : "/etc/subgid";
    std::ifstream file(filename);
    if (!file.is_open()) {
        return {};
    }

    return detail::parseSubuidRanges(file, uid, *username);
}

} // namespace detail

utils::error::Result<bool> needRunInNamespace()
{
    LINGLONG_TRACE("check need run in namespace");

    auto *caps = cap_get_proc();
    if (caps == nullptr) {
        return LINGLONG_ERR("failed to get capabilities");
    }

    cap_flag_value_t value;
    if (cap_get_flag(caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &value) == -1) {
        cap_free(caps);
        return LINGLONG_ERR("failed to get capabilities");
    }

    cap_free(caps);

    return value != CAP_SET;
}

// run command in namespace
// try to clone process with new user_namespaces and mount_namespaces
// and map root/root in namespace to current user/group, /etc/subuid and
// /etc/subgid file will be considered
utils::error::Result<int> runInNamespace(int argc, char **argv)
{
    LINGLONG_TRACE("run in namespace");

    auto entry = [](void *args) {
        auto *runInNamespaceArgs = static_cast<detail::RunInNamespaceArgs *>(args);
        auto &pair = runInNamespaceArgs->pair;
        close(pair[0]);

        auto closeFd = linglong::utils::finally::finally([&pair]() {
            if (pair[1] != -1) {
                close(pair[1]);
            }
        });

        while (write(pair[1], "1", 1) != 1) {
            if (errno == EINTR) {
                continue;
            }

            LogW("failed to write data to sync socket: {}", ::strerror(errno));
            return -1;
        }

        LogD("waiting mapping");

        char buf{ 0 };
        while (read(pair[1], &buf, 1) != 1) {
            if (errno == EINTR) {
                continue;
            }

            LogW("failed to read data from sync socket: {}", ::strerror(errno));
            return -1;
        }

        close(pair[1]);
        pair[1] = -1;

        LogD("run command");

        execvp(runInNamespaceArgs->argv[0], runInNamespaceArgs->argv);

        LogE("execvp failed: {}", ::strerror(errno));
        return -1;
    };

    std::array<int, 2> pair{};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.data()) == -1) {
        return LINGLONG_ERR(fmt::format("socketpair failed: {}", ::strerror(errno)));
    }

    auto closeSocket = linglong::utils::finally::finally([&pair]() {
        if (pair[0] != -1) {
            close(pair[0]);
        }

        if (pair[1] != -1) {
            close(pair[1]);
        }
    });

    constexpr auto stackSize = 1024 * 1024;
    auto *addr = mmap(nullptr,
                      stackSize,
                      PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK,
                      -1,
                      0);
    if (addr == MAP_FAILED) {
        return LINGLONG_ERR(
          fmt::format("failed to create stack for child process: {}", strerror(errno)));
    }

    auto recycle = linglong::utils::finally::finally([addr]() {
        munmap(addr, stackSize);
    });

    detail::RunInNamespaceArgs args{ argc, argv, pair };
    auto pid = clone(entry,
                     static_cast<std::byte *>(addr) + stackSize,
                     CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD,
                     &args);
    close(pair[1]);
    pair[1] = -1;

    if (pid < 0) {
        return LINGLONG_ERR(fmt::format("clone failed: {}", strerror(errno)));
    }

    LogD("waiting child {}", pid);

    char buf{ 0 };
    while (read(pair[0], &buf, 1) != 1) {
        if (errno == EINTR) {
            continue;
        }

        return LINGLONG_ERR(fmt::format("read failed: {}", ::strerror(errno)));
    }

    auto mappingTool = [](bool isUid, pid_t pid) -> utils::error::Result<void> {
        LINGLONG_TRACE("mapping tool");

        auto id = isUid ? geteuid() : getegid();
        LogD("mapping uid and gid, isUid: {}, id: {}", isUid, id);

        auto ranges = detail::getSubuidRange(id, isUid);
        if (!ranges) {
            return LINGLONG_ERR("failed to get subuid range", ranges.error());
        }

        utils::command::Cmd cmd(isUid ? "newuidmap" : "newgidmap");
        unsigned long containerID = 0;
        QStringList args = { QString::number(pid),
                             QString::number(containerID),
                             QString::number(id),
                             "1" };
        for (const auto &range : *ranges) {
            args << QString::number(containerID + 1) << range.subuid.c_str() << range.count.c_str();
            try {
                containerID += std::stoul(range.count);
            } catch (const std::invalid_argument &e) {
                return LINGLONG_ERR(fmt::format("invalid subuid count: {}", range.count));
            } catch (const std::out_of_range &e) {
                return LINGLONG_ERR(fmt::format("subuid count out of range: {}", range.count));
            }
        }

        auto res = cmd.exec(args);
        if (!res) {
            return LINGLONG_ERR("newuidmap failed", res.error());
        }

        return LINGLONG_OK;
    };

    auto res = mappingTool(true, pid);
    if (!res) {
        return LINGLONG_ERR("failed to mapping uid", res.error());
    }

    res = mappingTool(false, pid);
    if (!res) {
        return LINGLONG_ERR("failed to mapping gid", res.error());
    }

    if (write(pair[0], "1", 1) == -1) {
        return LINGLONG_ERR("write failed");
    }
    close(pair[0]);
    pair[0] = -1;

    int status{ 0 };
    while (true) {
        auto res = waitpid(pid, &status, 0);
        if (res == -1) {
            if (errno == EINTR) {
                continue;
            }

            return LINGLONG_ERR(fmt::format("waitpid failed {}", strerror(errno)));
        }

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            status = 128 + WTERMSIG(status);
        } else {
            status = -1;
        }

        LogD("child exit with status {}", status);
        break;
    }

    return status;
}

} // namespace linglong::utils
