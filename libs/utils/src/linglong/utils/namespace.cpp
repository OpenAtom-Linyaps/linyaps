// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "namespace.h"

#include "linglong/utils/command/cmd.h"
#include "linglong/utils/log/log.h"

#include <sys/capability.h>

#include <array>
#include <fstream>

#include <pwd.h>
#include <sched.h>
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
    struct passwd *result;
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

    auto filename = isUid ? "/etc/subuid" : "/etc/subgid";
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

    auto caps = cap_get_proc();
    if (!caps) {
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
utils::error::Result<int> runInNamespace(int argc, char *argv[])
{
    LINGLONG_TRACE("run in namespace");

    auto entry = [](void *args) {
        LINGLONG_TRACE("run in namespace entry");

        auto *runInNamespaceArgs = static_cast<detail::RunInNamespaceArgs *>(args);
        const auto &fd = runInNamespaceArgs->fd;

        write(fd, "1", 1);

        LogD("waiting mapping");

        char buf;
        if (read(fd, &buf, 1) != 1) {
            return -1;
        }
        close(fd);

        LogD("run command");

        execvp(runInNamespaceArgs->argv[0], runInNamespaceArgs->argv);

        LogD("execvp failed");
        return -1;
    };

    int pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
        return LINGLONG_ERR("socketpair failed");
    }

    detail::RunInNamespaceArgs args{ argc, argv, pair[1] };
    std::vector<char> stack(1024 * 1024);
    auto pid = clone((int (*)(void *))entry,
                     stack.data() + stack.size(),
                     CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD,
                     &args);
    if (pid < 0) {
        return LINGLONG_ERR(fmt::format("clone failed {}", strerror(errno)).c_str(), errno);
    }

    close(pair[1]);

    LogD("waiting child {}", pid);

    char buf;
    if (read(pair[0], &buf, 1) != 1) {
        return LINGLONG_ERR("read failed");
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
        int containerID = 0;
        QStringList args = { QString::number(pid),
                             QString::number(containerID),
                             QString::number(id),
                             "1" };
        for (const auto &range : *ranges) {
            args << QString::number(containerID + 1) << range.subuid.c_str() << range.count.c_str();
            containerID += std::stoi(range.count);
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

    int status;
    while (true) {
        auto res = waitpid(pid, &status, 0);
        if (res == -1) {
            if (errno == EINTR) {
                continue;
            }

            return LINGLONG_ERR(fmt::format("waitpid failed {}", strerror(errno)).c_str());
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