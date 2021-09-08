/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "container.h"

#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include <grp.h>
#include <sched.h>
#include <unistd.h>

#include <cerrno>

#include <utility>

#include "util/logger.h"
#include "util/filesystem.h"

#include "semaphore.h"

namespace {

// ###### Debug Utils

#define dump_uid_pid() \
    do { \
        logErr() << "getuid" << getuid() << "\n" \
                 << "geteuid" << geteuid() << "\n" \
                 << "getgid" << getgid() << "\n" \
                 << "getegid" << getegid() << "\n"; \
    } while (0)
//######

#define STACK_SIZE (1024 * 1024)

} // namespace

namespace linglong {

struct Container::ContainerPrivate {
public:
    ContainerPrivate(Runtime r, Container *parent)
        : hostRoot(r.root.path)
        , r(std::move(r))
    {
        if (!util::fs::is_dir(hostRoot) || !util::fs::exists(hostRoot)) {
            throw std::runtime_error((hostRoot + " not exist or not a dir").c_str());
        }
    }

    std::string hostRoot;
    Runtime r;

    int semID = -1;

public:
    //    int dropToUser(int uid, int gid)
    //    {
    //        setuid(uid);
    //        seteuid(uid);
    //        setgid(gid);
    //        setegid(gid);
    //        return 0;
    //    }

    int dropPermissions()
    {
        __gid_t newgid[1] = {getgid()};
        auto newuid = getuid();
        auto olduid = geteuid();

        if (0 == olduid) {
            setgroups(1, newgid);
        }
        seteuid(newuid);
        return 0;
    }

    int preLinks()
    {
        chdir("/");
        symlink("/usr/bin", "/bin");
        symlink("/usr/lib", "/lib");
        symlink("/usr/lib32", "/lib32");
        symlink("/usr/lib64", "/lib64");
        symlink("/usr/libx32", "/libx32");

        // FIXME: where is the /etc
        symlink("/usr/etc", "/etc");

        // default link
        symlink("/proc/kcore", "/dev/core");
        symlink("/proc/self/fd", "/dev/fd");
        symlink("/proc/self/fd/2", "/dev/stderr");
        symlink("/proc/self/fd/0", "/dev/stdin");
        symlink("/proc/self/fd/1", "/dev/stdout");
        return 0;
    }

    int preDefaultDevices() const
    {
        bool isInUserNamespace = false;

        for (const auto &ns : r.linux.namespaces) {
            switch (ns.type) {
            case CLONE_NEWUSER:
                isInUserNamespace = false;
            }
        }

        struct Device {
            std::string path;
            mode_t mode;
            dev_t dev;
        };
        std::vector<Device> list = {
            {"/dev/null", S_IFCHR | 0666, makedev(1, 3)},
            {"/dev/zero", S_IFCHR | 0666, makedev(1, 5)},
            {"/dev/full", S_IFCHR | 0666, makedev(1, 7)},
            {"/dev/random", S_IFCHR | 0666, makedev(1, 8)},
            {"/dev/urandom", S_IFCHR | 0666, makedev(1, 9)},
            {"/dev/tty", S_IFCHR | 0666, makedev(5, 0)},
        };

        if (isInUserNamespace) {
            for (auto const &dev : list) {
                auto path = hostRoot + dev.path;
                int ret = mknod(path.c_str(), dev.mode, dev.dev);
                if (0 != ret) {
                    logErr() << "mknod" << path << dev.mode << dev.dev << "failed with" << ret << errno;
                }
                chmod(path.c_str(), 0666);
                chown(path.c_str(), 0, 0);
            }
        } else {
            for (auto const &dev : list) {
                Mount m;
                m.destination = dev.path;
                m.type = "bind";
                m.options = std::vector<std::string> {"bind"};
                m.fsType = Mount::Bind;
                m.source = dev.path;
                this->mountNode(m);
            }
        }

        // FIXME: /dev/console is set up if terminal is enabled in the config by bind mounting the pseudoterminal slave to /dev/console.
        symlink("/dev/ptmx", "/dev/pts/ptmx");
        return 0;
    }

    int pivotRoot() const
    {
        chdir(hostRoot.c_str());
        syscall(SYS_pivot_root, ".", "ll-host");
        chroot(".");
        umount2("/ll-host", MNT_DETACH);
        return 0;
    }

    int mountNode(Mount m) const
    {
        struct stat sourceStat {
        };
        int ret = 0;
        std::error_code ec;

        if (m.fsType == Mount::Bind && 0 != lstat(m.source.c_str(), &sourceStat)) {
            logErr() << "lstat" << m.source << "failed";
        }

        auto destFullPath = this->hostRoot + m.destination;

        auto sourceFileInfo = util::fs::status(util::fs::path(m.source), ec);
        auto destFileInfo = util::fs::status(util::fs::path(destFullPath), ec);

        switch (sourceStat.st_mode & S_IFMT) {
        case S_IFCHR: {
            util::fs::create_directories(util::fs::path(destFullPath).parent_path(), 0755);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFSOCK: {
            util::fs::create_directories(util::fs::path(destFullPath).parent_path(), 0755);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFLNK: {
            util::fs::create_directories(util::fs::path(destFullPath).parent_path(), 0755);
            std::ofstream output(destFullPath);
            m.source = util::fs::read_symlink(util::fs::path(m.source)).string();
            break;
        }
        case S_IFREG: {
            util::fs::create_directories(util::fs::path(destFullPath).parent_path(), 0755);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFDIR:
            util::fs::create_directories(util::fs::path(destFullPath), 0755);
            break;
        default:
            logWar() << "!!source mode:" << m.source << (sourceStat.st_mode & S_IFMT);
            util::fs::create_directories(util::fs::path(destFullPath), 0755);
            break;
        }
        //        logErr() << m.options;

        for (const auto &opt : m.options) {
            if (opt.rfind("mode=", 0) == 0) {
                auto equalPos = opt.find('=');
                auto mode = opt.substr(equalPos + 1, opt.length() - equalPos - 1);
                chmod(destFullPath.c_str(), std::stoi(mode, nullptr, 8));
            }
        }

        uint32_t flags = m.flags;
        auto opts = util::str_vec_join(m.options, ',');

        logDbg() << "mount" << m.source << "to" << destFullPath << flags << opts;
        switch (m.fsType) {
        case Mount::Bind:
            chmod(destFullPath.c_str(), sourceStat.st_mode);
            chown(destFullPath.c_str(), sourceStat.st_uid, sourceStat.st_gid);
            ret = ::mount(m.source.c_str(),
                          destFullPath.c_str(),
                          nullptr,
                          MS_BIND,
                          nullptr);
            if (0 != ret) {
                break;
            }
            ret = ::mount(m.source.c_str(),
                          destFullPath.c_str(),
                          nullptr,
                          flags | MS_BIND | MS_REMOUNT,
                          opts.c_str());
            break;
        case Mount::Proc:
        case Mount::Devpts:
        case Mount::Mqueue:
        case Mount::Tmpfs:
        case Mount::Sysfs:
            ret = ::mount(m.source.c_str(),
                          destFullPath.c_str(),
                          m.type.c_str(),
                          flags,
                          opts.c_str());
            break;
        case Mount::Cgroup:
            logErr() << "unsupported cgroup mount";
            break;
        default:
            logErr() << "unsupported type" << m.type;
        }
        return ret;
    }
};

// int hookExec(const QString &path, const QStringList &args, const QStringList &env)
//{
//     auto targetArgc = args.length() + 1;
//     auto targetArgv = new const char *[targetArgc + 1];
//     QList<std::string> argvStringList;
//
//     argvStringList.push_back(path.toStdString());
//     targetArgv[0] = (char *)(argvStringList[0].c_str());
//
//     for (int i = 1; i < targetArgc; i++) {
//         argvStringList.push_back(args[i - 1].toStdString());
//         targetArgv[i] = (char *)(argvStringList[i].c_str());
//     }
//     targetArgv[targetArgc] = nullptr;
//     char *const *execArgv = const_cast<char **>(targetArgv);
//
//     auto targetEnvCount = env.length();
//     auto targetEnvList = new const char *[targetEnvCount + 1];
//     QList<std::string> envStringList;
//     for (int i = 0; i < targetEnvCount; i++) {
//         envStringList.push_back(env[i].toStdString());
//         targetEnvList[i] = (char *)(envStringList[i].c_str());
//     }
//     targetEnvList[targetEnvCount] = nullptr;
//     char *const *execEnvList = const_cast<char **>(targetEnvList);
//
//     logDbg() << "hookExec" << targetArgv[0] << execArgv[0] << execArgv[1] << execEnvList << "@"
//              << " with pid:" << getpid();
//
//     int execPid = fork();
//
//     if (execPid < 0) {
//         logErr() << "fork failed" << retErrString(execPid);
//         return -1;
//     }
//
//     if (execPid == 0) {
//         int ret = execve(targetArgv[0], execArgv, execEnvList);
//         if (0 != ret) {
//             logErr() << "execve failed" << retErrString(ret);
//         }
//         logErr() << "execve return" << retErrString(ret);
//         delete[] targetArgv;
//         delete[] targetEnvList;
//         return ret;
//     }
//
//     return waitpid(execPid, 0, 0);
// }

int entryProc(void *arg)
{
    auto &c = *reinterpret_cast<Container::ContainerPrivate *>(arg);

    auto newHostname = c.r.hostname;

    // FIXME: change HOSTNAME will broken XAUTH
    //    if (sethostname(newHostname.c_str(), strlen(newHostname.c_str())) == -1) {
    //        logErr() << "sethostname failed" << errnoString();
    //        return -1;
    //    }

    uint32_t flags = MS_REC | MS_SLAVE;
    int ret = mount(nullptr, "/", nullptr, flags, nullptr);
    if (0 != ret) {
        logErr() << "mount / failed" << util::retErrString(ret);
        return -1;
    }

    ret = mount("tmpfs", c.hostRoot.c_str(), "tmpfs", MS_NODEV | MS_NOSUID, nullptr);
    if (0 != ret) {
        logErr() << "mount" << c.hostRoot << "failed" << util::retErrString(ret);
        return -1;
    }

    chdir(c.hostRoot.c_str());
    for (auto const &m : c.r.mounts) {
        c.mountNode(m);
    }

    c.preDefaultDevices();

    ret = unshare(CLONE_NEWUSER);
    if (0 != ret) {
        logErr() << "unshare CLONE_NEWUSER failed" << util::retErrString(ret);
    }

    logDbg() << "wait parent write uid/gid map" << c.semID << std::flush;
    Semaphore s(c.semID);
    s.vrijgeven();
    s.passeren();

    c.pivotRoot();
    c.preLinks();

    chdir(c.r.process.cwd.c_str());

    int execPid = fork();

    if (execPid < 0) {
        logErr() << "fork failed" << util::retErrString(execPid);
        return -1;
    }

    if (execPid == 0) {
        seteuid(0);
        //        for (auto &prestart : c.r.hooks.poststart) {
        //            FIXME:!!!!!!
        //            hookExec(prestart.path, prestart.args, prestart.env);
        //        }

        c.dropPermissions();
        // PR_SET_PDEATHSIG will clear by drop permissions
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        logDbg() << "c.r.process.args:" << c.r.process.args;

        auto targetArgc = c.r.process.args.size();
        auto targetArgv = new const char *[targetArgc + 1];
        std::vector<std::string> argvStringList;
        for (size_t i = 0; i < targetArgc; i++) {
            argvStringList.push_back(c.r.process.args[i]);
            targetArgv[i] = (char *)(argvStringList[i].c_str());
        }
        targetArgv[targetArgc] = nullptr;
        char *const *execArgv = const_cast<char **>(targetArgv);

        auto targetEnvCount = c.r.process.env.size();
        auto targetEnvList = new const char *[targetEnvCount + 1];
        for (size_t i = 0; i < targetEnvCount; i++) {
            targetEnvList[i] = (char *)(c.r.process.env[i].c_str());
        }
        targetEnvList[targetEnvCount] = nullptr;
        char *const *execEnvList = const_cast<char **>(targetEnvList);

        logDbg() << "execve" << targetArgv[0] << execArgv[0] << execEnvList << "@" << c.r.process.cwd << " with pid:" << getpid();

        ret = execve(targetArgv[0], execArgv, execEnvList);
        if (0 != ret) {
            logErr() << "execve failed" << util::retErrString(ret);
        }
        logDbg() << "execve return" << util::retErrString(ret);
        delete[] targetArgv;
        delete[] targetEnvList;
        return ret;
    }

    c.dropPermissions();
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    while (true) {
        int childPid = waitpid(-1, nullptr, 0);
        if (childPid > 0) {
            logDbg() << "wait" << childPid << "exit";
        }
        if (childPid < 0) {
            logInf() << "all child exit" << childPid;
            return 0;
        }
    }
}

Container::Container(const Runtime &r)
    : dd_ptr(new ContainerPrivate(r, this))
{
}

int Container::start()
{
    char *stack;
    char *stackTop;

    stack = reinterpret_cast<char *>(
        mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
    if (stack == MAP_FAILED) {
        return -1;
    }

    stackTop = stack + STACK_SIZE;

    int flags = SIGCHLD | CLONE_NEWNS;

    for (auto const &n : dd_ptr->r.linux.namespaces) {
        switch (n.type) {
        case CLONE_NEWIPC:
        case CLONE_NEWUTS:
        case CLONE_NEWNS:
        case CLONE_NEWCGROUP:
        case CLONE_NEWPID:
        case CLONE_NEWNET:
            flags |= n.type;
        }
    }

    Semaphore s(getpid());
    s.init();
    dd_ptr->semID = getpid();

    logErr() << "prepare parent write" << dd_ptr->semID;

    int entryPid = clone(entryProc, stackTop, flags, (void *)dd_ptr.get());
    if (entryPid == -1) {
        logErr() << "clone failed" << util::retErrString(entryPid);
        return -1;
    }

    // NOTICE: log will not show
    logDbg() << "wait child start" << std::flush;
    s.passeren();

    logDbg() << "write uid_map and pid_map" << std::flush;
    // write uid map
    std::ofstream uidMapFile(util::format("/proc/%d/uid_map", entryPid));
    for (auto const &idMap : dd_ptr->r.linux.uidMappings) {
        uidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
    }
    uidMapFile.close();

    // write gid map
    std::ofstream gidMapFile(util::format("/proc/%d/gid_map", entryPid));
    for (auto const &idMap : dd_ptr->r.linux.gidMappings) {
        gidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
    }
    gidMapFile.close();

    logDbg() << "finish write uid_map and pid_map" << std::flush;
    s.vrijgeven();

    dd_ptr->dropPermissions();
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    if (waitpid(-1, nullptr, 0) < 0) {
        logErr() << "waitpid failed" << util::errnoString();
        return -1;
    }
    logInf() << "wait" << entryPid << "finish";

    return 0;
}

Container::~Container() = default;

} // namespace linglong