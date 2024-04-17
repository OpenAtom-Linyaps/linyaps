/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container.h"

#include "container/container_option.h"
#include "container/mount/filesystem_driver.h"
#include "container/mount/host_mount.h"
#include "container/seccomp.h"
#include "util/debug/debug.h"
#include "util/filesystem.h"
#include "util/logger.h"
#include "util/platform.h"
#include "util/semaphore.h"

#include <sys/epoll.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>

#include <cerrno>
#include <map>
#include <utility>

#include <fcntl.h>
#include <grp.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong {

static const std::string llDbusProxyBin = "/usr/bin/ll-dbus-proxy";

// start dbus proxy
static int StartDbusProxy(const Runtime &runtime)
{
    if (!(runtime.annotations.has_value() && runtime.annotations->dbus_proxy_info.has_value()
          && runtime.annotations->dbus_proxy_info->enable)) {
        logInf() << "dbus proxy disabled";
        return -1;
    }

    const auto &info = runtime.annotations->dbus_proxy_info;

    std::string socket_path = info->proxy_path;

    pid_t proxy_pid = fork();
    if (proxy_pid < 0) {
        logErr() << "fork to start dbus proxy failed:", util::errnoString();
        return -1;
    }

    if (0 == proxy_pid) {
        // FIXME: parent may dead before this return.
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        std::string bus_type = info->bus_type;
        std::string app_id = info->app_id;
        std::vector<std::string> name_fliter = info->name;
        std::vector<std::string> path_fliter = info->path;
        std::vector<std::string> interface_fliter = info->interface;
        std::string name_fliter_string = linglong::util::str_vec_join(name_fliter, ',');
        std::string path_fliter_string = linglong::util::str_vec_join(path_fliter, ',');
        std::string interface_fliter_string = linglong::util::str_vec_join(interface_fliter, ',');
        char const *const args[] = { llDbusProxyBin.c_str(),
                                     app_id.c_str(),
                                     bus_type.c_str(),
                                     socket_path.c_str(),
                                     name_fliter_string.c_str(),
                                     path_fliter_string.c_str(),
                                     interface_fliter_string.c_str(),
                                     NULL };
        int ret = execvp(args[0], (char **)args);
        logErr() << "start dbus proxy failed, ret=" << ret;
        exit(ret);
    } else {
        // FIXME: this call make 10ms lag at least
        if (util::fs::path(socket_path).wait_until_exsit() != 0) {
            logErr() << util::format("timeout! socketPath [\"%s\"] not exsit", socket_path.c_str());
            return -1;
        }
    }
    return 0;
}

int DropToNormalUser(int uid, int gid)
{
    setuid(uid);
    seteuid(uid);
    setgid(gid);
    setegid(gid);
    return 0;
}

static int ConfigUserNamespace(const linglong::Linux &linux, int initPid)
{
    std::string pid = "self";
    if (initPid > 0) {
        pid = util::format("%d", initPid);
    }

    logDbg() << "old uid:" << getuid() << "gid:" << getgid();
    logDbg() << "start write uid_map and pid_map" << initPid;

    // write uid map
    std::ofstream uidMapFile(util::format("/proc/%s/uid_map", pid.c_str()));
    for (auto const &idMap : linux.uidMappings) {
        uidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
    }
    uidMapFile.close();

    // write gid map
    auto setgroupsPath = util::format("/proc/%s/setgroups", pid.c_str());
    std::ofstream setgroupsFile(setgroupsPath);
    setgroupsFile << "deny";
    setgroupsFile.close();

    std::ofstream gidMapFile(util::format("/proc/%s/gid_map", pid.c_str()));
    for (auto const &idMap : linux.gidMappings) {
        gidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
    }
    gidMapFile.close();

    logDbg() << "new uid:" << getuid() << "gid:" << getgid();
    return 0;
}

// FIXME(iceyer): not work now
static int ConfigCgroupV2(const std::string &cgroupsPath,
                          const linglong::Resources &res,
                          int initPid)
{
    if (cgroupsPath.empty()) {
        logWan() << "skip with empty cgroupsPath";
        return 0;
    }

    auto writeConfig = [](const std::map<std::string, std::string> &cfgs) {
        for (auto const &cfg : cfgs) {
            logWan() << "ConfigCgroupV2" << cfg.first << cfg.second;
            std::ofstream cfgFile(cfg.first);
            cfgFile << cfg.second << std::endl;
            cfgFile.close();
        }
    };

    auto cgroupsRoot = util::fs::path(cgroupsPath);
    util::fs::create_directories(cgroupsRoot, 0755);

    int ret = mount("cgroup2", cgroupsRoot.string().c_str(), "cgroup2", 0u, nullptr);
    if (ret != 0) {
        logErr() << "mount cgroup failed" << util::RetErrString(ret);
        return -1;
    }

    // TODO: should sub path with pid?
    auto subCgroupRoot = cgroupsRoot / "ll-box";
    if (!util::fs::create_directories(util::fs::path(subCgroupRoot), 0755)) {
        logErr() << "create_directories subCgroupRoot failed" << util::errnoString();
        return -1;
    }

    auto subCgroupPath = [subCgroupRoot](const std::string &compents) -> std::string {
        return (subCgroupRoot / compents).string();
    };

    // config memory
    const auto memMax = res.memory.limit;

    if (memMax > 0) {
        const auto memSwapMax = res.memory.swap - memMax;
        const auto memLow = res.memory.reservation;
        writeConfig({
                { subCgroupPath("memory.max"), util::format("%d", memMax) },
                { subCgroupPath("memory.swap.max"), util::format("%d", memSwapMax) },
                { subCgroupPath("memory.low"), util::format("%d", memLow) },
        });
    }

    // config cpu
    const auto cpuPeriod = res.cpu.period;
    const auto cpuMax = res.cpu.quota;
    // convert from [2-262144] to [1-10000], 262144 is 2^18
    const auto cpuWeight = (1 + ((res.cpu.shares - 2) * 9999) / 262142);

    writeConfig({
            { subCgroupPath("cpu.max"), util::format("%d %d", cpuMax, cpuPeriod) },
            { subCgroupPath("cpu.weight"), util::format("%d", cpuWeight) },
    });

    // config pid
    writeConfig({
            { subCgroupPath("cgroup.procs"), util::format("%d", initPid) },
    });

    logDbg() << "move" << initPid << "to new cgroups";

    return 0;
}

inline void epoll_ctl_add(int epfd, int fd)
{
    static epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0)
        logWan() << util::errnoString();
}

// if wstatus says child exit normally, return true else false
static bool parse_wstatus(const int &wstatus, std::string &info)
{
    if (WIFEXITED(wstatus)) {
        auto code = WEXITSTATUS(wstatus);
        info = util::format("exited with code %d", code);
        return code == 0;
    } else if (WIFSIGNALED(wstatus)) {
        info = util::format("terminated by signal %d", WTERMSIG(wstatus));
        return false;
    } else {
        info = util::format("is dead with wstatus=%d", wstatus);
        return false;
    }
}

struct ContainerPrivate
{
public:
    ContainerPrivate(Runtime r, std::unique_ptr<util::MessageReader> reader, Container * /*parent*/)
        : hostRoot(r.root.path)
        , runtime(std::move(r))
        , nativeMounter(new HostMount)
        , overlayfsMounter(new HostMount)
        , fuseproxyMounter(new HostMount)
        , reader(std::move(reader))
    {
    }

    std::string hostRoot;

    Runtime runtime;

    //    bool use_delay_new_user_ns = false;
    bool useNewCgroupNs = false;

    Option option;

    uid_t hostUid = -1;
    gid_t hostGid = -1;

    std::unique_ptr<HostMount> nativeMounter;
    std::unique_ptr<HostMount> overlayfsMounter;
    std::unique_ptr<HostMount> fuseproxyMounter;

    HostMount *containerMounter = nullptr;

    std::unique_ptr<util::MessageReader> reader;

    std::map<int, std::string> pidMap;

public:
    static int DropPermissions()
    {
        __gid_t newgid[1] = { getgid() };
        auto newuid = getuid();
        auto olduid = geteuid();

        if (0 == olduid) {
            setgroups(1, newgid);
        }
        seteuid(newuid);
        return 0;
    }

    int PrepareLinks() const
    {
        chdir("/");

        if (option.linkLfs) {
            symlink("/usr/bin", "/bin");
            symlink("/usr/lib", "/lib");
            symlink("/usr/lib32", "/lib32");
            symlink("/usr/lib64", "/lib64");
            symlink("/usr/libx32", "/libx32");
        }

        // default link
        symlink("/proc/kcore", "/dev/core");
        symlink("/proc/self/fd", "/dev/fd");
        symlink("/proc/self/fd/2", "/dev/stderr");
        symlink("/proc/self/fd/0", "/dev/stdin");
        symlink("/proc/self/fd/1", "/dev/stdout");

        return 0;
    }

    int PrepareDefaultDevices() const
    {
        struct Device
        {
            std::string path;
            mode_t mode;
            dev_t dev;
        };

        std::vector<Device> list = {
            { "/dev/null", S_IFCHR | 0666, makedev(1, 3) },
            { "/dev/zero", S_IFCHR | 0666, makedev(1, 5) },
            { "/dev/full", S_IFCHR | 0666, makedev(1, 7) },
            { "/dev/random", S_IFCHR | 0666, makedev(1, 8) },
            { "/dev/urandom", S_IFCHR | 0666, makedev(1, 9) },
            { "/dev/tty", S_IFCHR | 0666, makedev(5, 0) },
        };

        // TODO(iceyer): not work now
        if (!option.rootless) {
            for (auto const &dev : list) {
                auto path = hostRoot + dev.path;
                int ret = mknod(path.c_str(), dev.mode, dev.dev);
                if (0 != ret) {
                    logErr() << "mknod" << path << dev.mode << dev.dev << "failed with"
                             << util::RetErrString(ret);
                }
                chmod(path.c_str(), dev.mode | 0xFFFF);
                chown(path.c_str(), 0, 0);
            }
        } else {
            for (auto const &dev : list) {
                Mount mount;
                mount.destination = dev.path;
                mount.type = "bind";
                mount.data = std::vector<std::string>{};
                mount.flags = MS_BIND;
                mount.fsType = Mount::Bind;
                mount.source = dev.path;

                this->containerMounter->MountNode(mount);
            }
        }

        // FIXME(iceyer): /dev/console is set up if terminal is enabled in the config by bind
        // mounting the pseudoterminal slave to /dev/console.
        symlink("/dev/pts/ptmx", (this->hostRoot + "/dev/ptmx").c_str());

        return 0;
    }

    void waitChildAndExec()
    {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGTERM);
        // FIXME: add more signals.

        /* Block signals so that they aren't handled
           according to their default dispositions. */

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
            logWan() << "sigprocmask block";

        int sfd = signalfd(-1, &mask, 0);
        if (sfd == -1)
            logWan() << "signalfd";

        auto epfd = epoll_create(1);
        epoll_ctl_add(epfd, sfd);
        if (reader.get() != nullptr)
            epoll_ctl_add(epfd, reader->fd);

        for (;;) {
            struct epoll_event events[10];
            int event_cnt = epoll_wait(epfd, events, 5, -1);
            for (int i = 0; i < event_cnt; i++) {
                const auto &event = events[i];
                if (event.data.fd == sfd) {
                    struct signalfd_siginfo fdsi;
                    ssize_t s = read(sfd, &fdsi, sizeof(fdsi));
                    if (s != sizeof(fdsi)) {
                        logWan() << "error read from signal fd";
                    }
                    if (fdsi.ssi_signo == SIGCHLD) {
                        logWan() << "recive SIGCHLD";

                        int wstatus;
                        while (int child = waitpid(-1, &wstatus, WNOHANG)) {
                            if (child > 0) {
                                std::string info;
                                auto normal = parse_wstatus(wstatus, info);
                                info = util::format("child [%d] [%s].", child, info.c_str());
                                if (normal) {
                                    logDbg() << info;
                                } else {
                                    logWan() << info;
                                }
                                auto it = pidMap.find(child);
                                if (it != pidMap.end()) {
                                    if (reader.get() != nullptr)
                                        reader->writeChildExit(child, it->second, wstatus, info);
                                    pidMap.erase(it);
                                }
                            } else if (child < 0) {
                                if (errno == ECHILD) {
                                    logDbg() << util::format("no child to wait");
                                    return;
                                } else {
                                    auto str = util::errnoString();
                                    logErr() << util::format("waitpid failed, %s", str.c_str());
                                    return;
                                }
                            }
                        }
                    } else if (fdsi.ssi_signo == SIGTERM) {
                        // FIXME: box should exit with failed return code.
                        logWan() << util::format("Terminated\n");
                        return;
                    } else {
                        logWan() << util::format("Read unexpected signal [%d]\n", fdsi.ssi_signo);
                    }
                } else if (reader.get() != nullptr && event.data.fd == reader->fd) {
                    auto json = reader->read();
                    if (json.empty()) {
                        break;
                    }
                    auto process = json.get<Process>();
                    forkAndExecProcess(process, true);
                } else {
                    logWan() << "Unknown fd";
                }
            }
        }
        return;
    }

    bool forkAndExecProcess(const Process process, bool unblock = false)
    {
        // FIXME: parent may dead before this return.
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        int pid = fork();
        if (pid < 0) {
            logErr() << "fork failed" << util::RetErrString(pid);
            return false;
        }

        if (0 == pid) {
            if (unblock) {
                // FIXME: As we use signalfd, we have to block signal, but child created by fork
                // will inherit blocked signal set, so we have to unblock it. This is just a
                // workaround.
                sigset_t mask;
                sigfillset(&mask);
                if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
                    logWan() << "sigprocmask unblock";
            }
            logDbg() << "process.args:" << process.args;

            int ret;
            ret = chdir(process.cwd.c_str());
            if (ret) {
                logErr() << "failed to chdir to" << process.cwd.c_str();
            }

            // for PATH
            for (const auto &env : process.env) {
                auto kv = util::str_spilt(env, "=");
                if (kv.size() == 2)
                    setenv(kv.at(0).c_str(), kv.at(1).c_str(), 1);
                else if (kv.size() == 1) {
                    setenv(kv.at(0).c_str(), "", 1);
                } else {
                    logWan() << "Unknown env:" << env;
                }
            }

            logInf() << "start exec process";
            ret = util::Exec(process.args, process.env);
            if (0 != ret) {
                logErr() << "exec failed" << util::RetErrString(ret);
            }
            exit(ret);
        } else {
            pidMap.insert(make_pair(pid, process.args[0]));
        }

        return true;
    }

    int PivotRoot() const
    {
        int ret = -1;
        chdir(hostRoot.c_str());

        if (option.rootless && runtime.annotations->overlayfs.has_value()) {
            int flag = MS_MOVE;
            ret = mount(".", "/", nullptr, flag, nullptr);
            if (0 != ret) {
                logErr() << "mount / failed" << util::RetErrString(ret);
                return -1;
            }

            ret = chroot(".");
            if (0 != ret) {
                logErr() << "chroot . failed" << util::RetErrString(ret);
                return -1;
            }
        } else {
            int flag = MS_BIND | MS_REC;
            ret = mount(".", ".", "bind", flag, nullptr);
            if (0 != ret) {
                logErr() << "mount / failed" << util::RetErrString(ret);
                return -1;
            }

            auto llHostFilename = "ll-host";

            auto llHostPath = hostRoot + "/" + llHostFilename;

            mkdir(llHostPath.c_str(), 0755);

            ret = syscall(SYS_pivot_root, hostRoot.c_str(), llHostPath.c_str());
            if (0 != ret) {
                logErr() << "SYS_pivot_root failed" << hostRoot << util::errnoString() << errno
                         << ret;
                return -1;
            }

            chdir("/");
            ret = chroot(".");
            if (0 != ret) {
                logErr() << "chroot failed" << hostRoot << util::errnoString() << errno;
                return -1;
            }

            chdir("/");
            umount2(llHostFilename, MNT_DETACH);
        }

        return 0;
    }

    int PrepareRootfs()
    {
        auto PrepareOverlayfsRootfs = [&](const AnnotationsOverlayfs &overlayfs) -> int {
            nativeMounter->Setup(new NativeFilesystemDriver(overlayfs.lower_parent));

            util::str_vec lowerDirs = {};
            int prefixIndex = 0;
            for (auto mount : overlayfs.mounts) {
                auto prefix = util::fs::path(util::format("/%d", prefixIndex));
                mount.destination = (prefix / mount.destination).string();
                if (0 == nativeMounter->MountNode(mount)) {
                    lowerDirs.push_back((util::fs::path(overlayfs.lower_parent) / prefix).string());
                }
                ++prefixIndex;
            }

            overlayfsMounter->Setup(new OverlayfsFuseFilesystemDriver(lowerDirs,
                                                                      overlayfs.upper,
                                                                      overlayfs.workdir,
                                                                      hostRoot));

            containerMounter = overlayfsMounter.get();
            return -1;
        };

        auto PrepareFuseProxyRootfs = [&](const AnnotationsOverlayfs &overlayfs) -> int {
            nativeMounter->Setup(new NativeFilesystemDriver(overlayfs.lower_parent));

            util::str_vec mounts = {};
            for (auto const &mount : overlayfs.mounts) {
                auto mountItem =
                        util::format("%s:%s\n", mount.source.c_str(), mount.destination.c_str());
                mounts.push_back(mountItem);
            }

            fuseproxyMounter->Setup(new FuseProxyFilesystemDriver(mounts, hostRoot));

            containerMounter = fuseproxyMounter.get();
            return -1;
        };

        auto PrepareNativeRootfs = [&](const AnnotationsNativeRootfs &native) -> int {
            nativeMounter->Setup(new NativeFilesystemDriver(runtime.root.path));

            for (const auto &mount : native.mounts) {
                nativeMounter->MountNode(mount);
            }

            containerMounter = nativeMounter.get();
            return -1;
        };

        if (runtime.annotations.has_value() && runtime.annotations->overlayfs.has_value()) {
            auto env = getenv("LL_BOX_FS_BACKEND");
            if (env && std::string(env) == "fuse-proxy") {
                return PrepareFuseProxyRootfs(runtime.annotations->overlayfs.value());
            } else {
                return PrepareOverlayfsRootfs(runtime.annotations->overlayfs.value());
            }
        } else {
            return PrepareNativeRootfs(runtime.annotations->native.has_value()
                                               ? runtime.annotations->native.value()
                                               : AnnotationsNativeRootfs());
        }

        return -1;
    }

    int MountContainerPath()
    {
        if (runtime.mounts.has_value()) {
            for (auto const &mount : runtime.mounts.value()) {
                containerMounter->MountNode(mount);
            }
        };

        return 0;
    }
};

int HookExec(const Hook &hook)
{
    int execPid = fork();
    if (execPid < 0) {
        logErr() << "fork failed" << util::RetErrString(execPid);
        return -1;
    }

    if (execPid == 0) {
        util::str_vec argStrVec;
        argStrVec.push_back(hook.path);

        std::copy(hook.args->begin(), hook.args->end(), std::back_inserter(argStrVec));

        util::Exec(argStrVec, hook.env.value_or(std::vector<std::string>{}));

        exit(0);
    }

    return waitpid(execPid, nullptr, 0);
}

int NonePrivilegeProc(void *arg)
{
    auto &containerPrivate = *reinterpret_cast<ContainerPrivate *>(arg);

    if (containerPrivate.option.rootless) {
        // TODO(iceyer): use option

        Linux linux;
        IDMap idMap;

        idMap.containerID = containerPrivate.hostUid;
        idMap.hostID = 0;
        idMap.size = 1;
        linux.uidMappings.push_back(idMap);

        idMap.containerID = containerPrivate.hostGid;
        idMap.hostID = 0;
        idMap.size = 1;
        linux.gidMappings.push_back(idMap);

        ConfigUserNamespace(linux, 0);
    }

    auto ret = mount("proc", "/proc", "proc", 0, nullptr);
    if (0 != ret) {
        logErr() << "mount proc failed" << util::RetErrString(ret);
        return -1;
    }

    if (containerPrivate.runtime.hooks.has_value()
        && containerPrivate.runtime.hooks->prestart.has_value()) {
        for (auto const &preStart : *containerPrivate.runtime.hooks->prestart) {
            HookExec(preStart);
        }
    }

    if (!containerPrivate.option.rootless) {
        seteuid(0);
        // todo: check return value
        ConfigSeccomp(containerPrivate.runtime.linux.seccomp);
        ContainerPrivate::DropPermissions();
    }

    containerPrivate.forkAndExecProcess(containerPrivate.runtime.process);

    containerPrivate.waitChildAndExec();
    return 0;
}

void sigtermHandler(int)
{
    exit(-1);
}

int EntryProc(void *arg)
{
    auto &containerPrivate = *reinterpret_cast<ContainerPrivate *>(arg);

    if (containerPrivate.option.rootless) {
        ConfigUserNamespace(containerPrivate.runtime.linux, 0);
    }

    // FIXME: change HOSTNAME will broken XAUTH
    auto new_hostname = containerPrivate.runtime.hostname;
    //    if (sethostname(new_hostname.c_str(), strlen(new_hostname.c_str())) == -1) {
    //        logErr() << "sethostname failed" << util::errnoString();
    //        return -1;
    //    }

    uint32_t flags = MS_REC | MS_SLAVE;
    int ret = mount(nullptr, "/", nullptr, flags, nullptr);
    if (0 != ret) {
        logErr() << "mount / failed" << util::RetErrString(ret);
        return -1;
    }

    std::string container_root = containerPrivate.runtime.annotations->container_root_path;
    flags = MS_NODEV | MS_NOSUID;
    ret = mount("tmpfs", container_root.c_str(), "tmpfs", flags, nullptr);
    if (0 != ret) {
        logErr() << util::format("mount container root (%s) failed:", container_root.c_str())
                 << util::RetErrString(ret);
        return -1;
    }

    // NOTE(iceyer): it's not standard oci action
    containerPrivate.PrepareRootfs();

    containerPrivate.MountContainerPath();

    if (containerPrivate.useNewCgroupNs) {
        ConfigCgroupV2(containerPrivate.runtime.linux.cgroupsPath,
                       containerPrivate.runtime.linux.resources,
                       getpid());
    }

    containerPrivate.PrepareDefaultDevices();

    containerPrivate.PivotRoot();

    containerPrivate.PrepareLinks();

    if (!containerPrivate.option.rootless) {
        auto unshareFlags = 0;
        // TODO(iceyer): no need user namespace in setuid
        //        if (c.use_delay_new_user_ns) {
        //            unshare_flags |= CLONE_NEWUSER;
        //        }
        if (containerPrivate.useNewCgroupNs) {
            unshareFlags |= CLONE_NEWCGROUP;
        }
        if (unshareFlags) {
            ret = unshare(unshareFlags);
            if (0 != ret) {
                logErr() << "unshare failed" << unshareFlags << util::RetErrString(ret);
            }
        }
        //        if (c.use_delay_new_user_ns) {
        //            s.vrijgeven();
        //            s.passeren();
        //        }
    }

    int nonePrivilegeProcFlag = SIGCHLD | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNS;

    int noPrivilegePid = util::PlatformClone(NonePrivilegeProc, nonePrivilegeProcFlag, arg);
    if (noPrivilegePid < 0) {
        logErr() << "clone failed" << util::RetErrString(noPrivilegePid);
        return -1;
    }

    ContainerPrivate::DropPermissions();

    // FIXME: parent may dead before this return.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell

    containerPrivate.reader.reset();
    signal(SIGTERM, sigtermHandler);
    util::WaitAllUntil(noPrivilegePid);
    return -1;
}

Container::Container(const Runtime &r, std::unique_ptr<util::MessageReader> reader)
    : dd_ptr(new ContainerPrivate(r, std::move(reader), this))
{
}

int Container::Start(const Option &option)
{
    auto &contanerPrivate = *reinterpret_cast<ContainerPrivate *>(dd_ptr.get());
    contanerPrivate.option = option;

    if (option.rootless) {
        contanerPrivate.hostUid = geteuid();
        contanerPrivate.hostGid = getegid();
    }

    int flags = SIGCHLD | CLONE_NEWNS;

    for (auto const &n : contanerPrivate.runtime.linux.namespaces) {
        switch (n.type) {
        case CLONE_NEWIPC:
        case CLONE_NEWUTS:
        case CLONE_NEWNS:
        case CLONE_NEWPID:
        case CLONE_NEWNET:
            flags |= n.type;
            break;
        case CLONE_NEWUSER:
            //            dd_ptr->use_delay_new_user_ns = true;
            break;
        case CLONE_NEWCGROUP:
            contanerPrivate.useNewCgroupNs = true;
            break;
        default:
            return -1;
        }
    }

    if (option.rootless) {
        flags |= CLONE_NEWUSER;
    }

    StartDbusProxy(contanerPrivate.runtime);

    int entryPid = util::PlatformClone(EntryProc, flags, (void *)dd_ptr.get());
    if (entryPid < 0) {
        logErr() << "clone failed" << util::RetErrString(entryPid);
        return -1;
    }

    contanerPrivate.reader.reset();

    // FIXME: maybe we need c.opt.child_need_wait?

    ContainerPrivate::DropPermissions();

    // FIXME: parent may dead before this return.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    util::WaitAllUntil(entryPid);

    return 0;
}

Container::~Container() = default;

} // namespace linglong
