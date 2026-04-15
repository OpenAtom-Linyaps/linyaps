// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "namespace.h"

#include "linglong/common/error.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <sys/capability.h>
#include <sys/prctl.h>

#include <fstream>
#include <sstream>

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

utils::error::Result<void> writeMapFile(pid_t pid, bool isUid, const std::string &content)
{
    LINGLONG_TRACE("write map file");

    const auto path = fmt::format("/proc/{}/{}_map", pid, isUid ? "uid" : "gid");
    std::ofstream file(path);
    if (!file.is_open()) {
        return LINGLONG_ERR(fmt::format("failed to open {}", path));
    }

    file << content;
    if (!file) {
        return LINGLONG_ERR(fmt::format("failed to write {}", path));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> denySetGroups(pid_t pid)
{
    LINGLONG_TRACE("deny setgroups");

    const auto path = fmt::format("/proc/{}/setgroups", pid);
    std::ofstream file(path);
    if (!file.is_open()) {
        return LINGLONG_ERR(fmt::format("failed to open {}", path));
    }

    file << "deny";
    if (!file) {
        return LINGLONG_ERR(fmt::format("failed to write {}", path));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> writeSingleIdMap(pid_t pid, bool isUid, uid_t hostID, uid_t namespaceID)
{
    LINGLONG_TRACE("write single id map");

    if (!isUid) {
        auto setGroups = denySetGroups(pid);
        if (!setGroups) {
            return LINGLONG_ERR("failed to deny setgroups", setGroups.error());
        }
    }

    return writeMapFile(pid, isUid, fmt::format("{} {} 1\n", namespaceID, hostID));
}

utils::error::Result<void> prepareAmbientCapabilities()
{
    LINGLONG_TRACE("prepare ambient capabilities");

    if (prctl(PR_SET_KEEPCAPS, 1L, 0L, 0L, 0L) == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to keep capabilities: {}", common::error::errorString(errno)));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> raiseAmbientCapabilities()
{
    LINGLONG_TRACE("raise ambient capabilities");

    auto *caps = cap_get_proc();
    if (caps == nullptr) {
        return LINGLONG_ERR("failed to get capabilities");
    }

    auto freeCaps = linglong::utils::finally::finally([caps]() {
        cap_free(caps);
    });

    cap_value_t capList[] = { CAP_SYS_ADMIN, CAP_DAC_OVERRIDE };
    constexpr auto capCount = static_cast<int>(sizeof(capList) / sizeof(capList[0]));
    if (cap_set_flag(caps, CAP_PERMITTED, capCount, capList, CAP_SET) == -1
        || cap_set_flag(caps, CAP_INHERITABLE, capCount, capList, CAP_SET) == -1
        || cap_set_flag(caps, CAP_EFFECTIVE, capCount, capList, CAP_SET) == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to set capability flags: {}", common::error::errorString(errno)));
    }

    if (cap_set_proc(caps) == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to apply capabilities: {}", common::error::errorString(errno)));
    }

    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_SYS_ADMIN, 0L, 0L) == -1) {
        return LINGLONG_ERR(fmt::format("failed to raise ambient CAP_SYS_ADMIN: {}",
                                        common::error::errorString(errno)));
    }

    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_DAC_OVERRIDE, 0L, 0L) == -1) {
        return LINGLONG_ERR(fmt::format("failed to raise ambient CAP_DAC_OVERRIDE: {}",
                                        common::error::errorString(errno)));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> mappingTool(pid_t pid, bool isUid, uid_t namespaceID, bool autoMapping)
{
    LINGLONG_TRACE("mapping tool");

    utils::Cmd cmd(isUid ? "newuidmap" : "newgidmap");
    auto hostID = isUid ? geteuid() : getegid();
    LogD("mapping uid and gid, isUid: {}, hostID: {}, id: {}, auto: {}",
         isUid,
         hostID,
         namespaceID,
         autoMapping);

    if (!autoMapping || !cmd.exists()) {
        return writeSingleIdMap(pid, isUid, hostID, namespaceID);
    }

    auto ranges = getSubuidRange(hostID, isUid);
    if (!ranges) {
        return LINGLONG_ERR("failed to get subuid range", ranges.error());
    }

    unsigned long containerID = namespaceID;
    auto args = std::vector<std::string>{ std::to_string(pid),
                                          std::to_string(containerID),
                                          std::to_string(hostID),
                                          "1" };
    for (const auto &range : *ranges) {
        unsigned long count = 0;
        try {
            count = std::stoul(range.count);
        } catch (const std::invalid_argument &e) {
            return LINGLONG_ERR(fmt::format("invalid subuid count: {}", range.count));
        } catch (const std::out_of_range &e) {
            return LINGLONG_ERR(fmt::format("subuid count out of range: {}", range.count));
        }

        args.insert(
          args.end(),
          { std::to_string(containerID + 1), range.subuid, std::to_string(count - namespaceID) });
        containerID += count;
    }

    auto res = cmd.exec(args);
    if (!res) {
        return LINGLONG_ERR(fmt::format("{} failed", isUid ? "newuidmap" : "newgidmap"),
                            res.error());
    }

    return LINGLONG_OK;
}

int runInNamespaceEntry(void *args)
{
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

        LogW("failed to write data to sync socket: {}", common::error::errorString(errno));
        return -1;
    }

    LogD("waiting mapping");

    char buf{ 0 };
    while (read(pair[1], &buf, 1) != 1) {
        if (errno == EINTR) {
            continue;
        }

        LogW("failed to read data from sync socket: {}", common::error::errorString(errno));
        return -1;
    }

    close(pair[1]);
    pair[1] = -1;

    auto targetUid = runInNamespaceArgs->uid.value_or(0);
    if (targetUid != 0) {
        auto res = prepareAmbientCapabilities();
        if (!res) {
            LogE("failed to prepare ambient capabilities for uid {}: {}",
                 targetUid,
                 res.error().message());
            return -1;
        }

        if (setuid(targetUid) == -1) {
            LogE("setuid failed: {}", common::error::errorString(errno));
            return -1;
        }

        res = raiseAmbientCapabilities();
        if (!res) {
            LogE("failed to grant ambient capabilities for uid {}: {}",
                 targetUid,
                 res.error().message());
            return -1;
        }
    }

    if (runInNamespaceArgs->gid && *runInNamespaceArgs->gid != 0) {
        if (setgid(*runInNamespaceArgs->gid) == -1) {
            LogE("setgid failed: {}", common::error::errorString(errno));
            return -1;
        }
    }

    LogD("run command {}", runInNamespaceArgs->argv[0]);
    for (int i = 1; i < runInNamespaceArgs->argc; ++i) {
        LogD("argv[{}]: {}", i, runInNamespaceArgs->argv[i]);
    }

    execvp(runInNamespaceArgs->argv[0], runInNamespaceArgs->argv);

    LogE("execvp failed: {}", common::error::errorString(errno));
    return -1;
}

} // namespace detail

utils::error::Result<std::string> getSelfExe()
{
    LINGLONG_TRACE("get self executable path");

    std::vector<char> pathBuf(256, '\0');
    while (true) {
        auto size = readlink("/proc/self/exe", pathBuf.data(), pathBuf.size());
        if (size == -1) {
            return LINGLONG_ERR(
              fmt::format("failed to read /proc/self/exe: {}", common::error::errorString(errno)));
        }

        if (static_cast<std::size_t>(size) < pathBuf.size()) {
            return std::string(pathBuf.data(), static_cast<std::size_t>(size));
        }

        pathBuf.resize(pathBuf.size() * 2, '\0');
    }
}

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
// if uid/gid given, map uid/gid to current user/group in namespace and
// gain CAP_SYS_ADMIN/CAP_DAC_OVERRIDE capabilities, otherwise
// map root/root in namespace to current user/group, and try to auto map subuid/subgid
utils::error::Result<int>
runInNamespace(int argc, char **argv, std::optional<uid_t> uid, std::optional<gid_t> gid)
{
    LINGLONG_TRACE("run in namespace");

    std::array<int, 2> pair{ -1, -1 };
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair.data()) == -1) {
        return LINGLONG_ERR(
          fmt::format("socketpair failed: {}", common::error::errorString(errno)));
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
        return LINGLONG_ERR(fmt::format("failed to create stack for child process: {}",
                                        common::error::errorString(errno)));
    }

    auto recycle = linglong::utils::finally::finally([addr]() {
        munmap(addr, stackSize);
    });

    detail::RunInNamespaceArgs args{ argc, argv, pair, uid, gid };
    auto pid = clone(detail::runInNamespaceEntry,
                     static_cast<std::byte *>(addr) + stackSize,
                     CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD,
                     &args);
    close(pair[1]);
    pair[1] = -1;

    if (pid < 0) {
        return LINGLONG_ERR(fmt::format("clone failed: {}", common::error::errorString(errno)));
    }

    LogD("waiting child {}", pid);

    char buf{ 0 };
    while (read(pair[0], &buf, 1) != 1) {
        if (errno == EINTR) {
            continue;
        }

        return LINGLONG_ERR(fmt::format("read failed: {}", common::error::errorString(errno)));
    }

    auto res = detail::mappingTool(pid, true, uid.value_or(0), !uid.has_value());
    if (!res) {
        return LINGLONG_ERR("failed to mapping uid", res.error());
    }

    res = detail::mappingTool(pid, false, gid.value_or(0), !gid.has_value());
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

            return LINGLONG_ERR(
              fmt::format("waitpid failed {}", common::error::errorString(errno)));
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

void checkPauseDebugger()
{
    const char *trace = getenv("TRACE_ME");

    if (trace != NULL && trace[0] == '1') {
        pid_t pid = getpid();

        fprintf(stderr, "\n======================================================\n");
        fprintf(stderr,
                "[DEBUG] TRACE_ME triggered! Process suspended waiting for debugger attachment.\n");
        fprintf(stderr, "[DEBUG] Current process PID: %d\n", pid);
        fprintf(stderr, "[DEBUG] Please execute the following command in another terminal:\n");
        fprintf(stderr, "        sudo gdb -p %d\n", pid);
        fprintf(stderr, "======================================================\n\n");

        // Send SIGSTOP signal to pause itself
        // This will suspend the process until it receives SIGCONT signal (GDB attach and continue
        // will send it)
        raise(SIGSTOP);
    }
}

} // namespace linglong::utils
