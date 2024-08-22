/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container.h"

#include <fcntl.h>
#include <grp.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <map>
#include <utility>

#include "container/helper.h"
#include "container/mount/host_mount.h"
#include "util/logger.h"
#include "util/platform.h"

namespace linglong {

namespace {
int ConfigUserNamespace(const linglong::Linux &linux, int initPid) {
  std::string pid = "self";
  if (initPid > 0) {
    pid = util::format("%d", initPid);
  }

  logDbg() << "old uid:" << getuid() << "gid:" << getgid();
  logDbg() << "start write uid_map and pid_map" << initPid;

  // write uid map
  auto uidMap = util::format("/proc/%s/uid_map", pid.c_str());
  std::ofstream uidMapFile(uidMap);
  if (!uidMapFile.is_open()) {
    logErr() << "couldn't open file" << uidMap;
    return -1;
  }

  for (auto const &idMap : linux.uidMappings) {
    uidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID,
                               idMap.size);
  }
  uidMapFile.close();

  // write gid map
  auto setgroupsPath = util::format("/proc/%s/setgroups", pid.c_str());
  std::ofstream setgroupsFile(setgroupsPath);
  if (!setgroupsFile.is_open()) {
    logErr() << "couldn't open file" << setgroupsPath;
    return -1;
  }
  setgroupsFile << "deny";
  setgroupsFile.close();

  auto gidMap = util::format("/proc/%s/gid_map", pid.c_str());
  std::ofstream gidMapFile(gidMap);
  if (!gidMapFile.is_open()) {
    logErr() << "couldn't open file" << gidMap;
    return -1;
  }

  for (auto const &idMap : linux.gidMappings) {
    gidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID,
                               idMap.size);
  }
  gidMapFile.close();

  logDbg() << "new uid:" << getuid() << "gid:" << getgid();
  return 0;
}
}  // namespace

// FIXME(iceyer): not work now
static int ConfigCgroupV2(const std::string &cgroupsPath,
                          const linglong::Resources &res, int initPid) {
  if (cgroupsPath.empty()) {
    logWan() << "skip with empty cgroupsPath";
    return 0;
  }

  auto writeConfig = [](const std::map<std::string, std::string> &cfgs) {
    for (auto const &cfg : cfgs) {
      logWan() << "ConfigCgroupV2" << cfg.first << cfg.second;
      std::ofstream cfgFile(cfg.first);
      if (!cfgFile.is_open()) {
        logErr() << "config file isn't open" << cfg.first;
        return false;
      }

      cfgFile << cfg.second << std::endl;
      cfgFile.close();
    }

    return true;
  };

  auto create_directories = [](const std::filesystem::path &path,
                               std::filesystem::perms perm,
                               std::error_code &ec) {
    if (!std::filesystem::create_directories(path, ec)) {
      logErr() << "couldn't create directories " << path.string()
               << ", error:" << ec.message();
      return;
    }

    std::filesystem::permissions(path, perm,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
      logErr() << "set " << path.string()
               << "permission error:" << ec.message();
      return;
    }
  };

  const auto dirPerm =
      std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
      std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
      std::filesystem::perms::others_exec;  // 0755
  std::error_code ec;

  auto cgroupsRoot = std::filesystem::path{cgroupsPath};
  create_directories(cgroupsRoot, dirPerm, ec);
  if (ec) {
    return -1;
  }

  if (auto ret = ::mount("cgroup2", cgroupsRoot.string().c_str(), "cgroup2", 0U,
                         nullptr);
      ret == -1) {
    logErr() << "mount cgroup failed:" << util::errnoString();
    return -1;
  }

  // TODO: should sub path with pid?
  auto subCgroupRoot = cgroupsRoot / "ll-box";
  create_directories(subCgroupRoot, dirPerm, ec);
  if (ec) {
    return -1;
  }

  auto subCgroupPath =
      [subCgroupRoot](const std::string &compents) -> std::string {
    return (subCgroupRoot / compents).string();
  };

  // config memory
  const auto memMax = res.memory.limit;

  if (memMax > 0) {
    const auto memSwapMax = res.memory.swap - memMax;
    const auto memLow = res.memory.reservation;
    auto ret = writeConfig({
        {subCgroupPath("memory.max"), util::format("%d", memMax)},
        {subCgroupPath("memory.swap.max"), util::format("%d", memSwapMax)},
        {subCgroupPath("memory.low"), util::format("%d", memLow)},
    });

    if (!ret) {
      logErr() << "write memory config failed";
      return -1;
    }
  }

  // config cpu
  const auto cpuPeriod = res.cpu.period;
  const auto cpuMax = res.cpu.quota;
  // convert from [2-262144] to [1-10000], 262144 is 2^18
  const auto cpuWeight = (1 + ((res.cpu.shares - 2) * 9999) / 262142);

  {
    auto ret = writeConfig({
        {subCgroupPath("cpu.max"), util::format("%d %d", cpuMax, cpuPeriod)},
        {subCgroupPath("cpu.weight"), util::format("%d", cpuWeight)},
    });

    if (!ret) {
      logErr() << "write cpu config failed";
      return -1;
    }
  }

  // config pid
  {
    auto ret = writeConfig({
        {subCgroupPath("cgroup.procs"), util::format("%d", initPid)},
    });

    if (!ret) {
      logErr() << "write cgroup config failed";
      return -1;
    }
  }

  logDbg() << "move" << initPid << "to new cgroups";
  return 0;
}

inline void epoll_ctl_add(int epfd, int fd) {
  static epoll_event ev = {};
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
  if (ret != 0) {
    logWan() << util::errnoString();
  }
}

// if wstatus says child exit normally, return true else false
static bool parse_wstatus(const int &wstatus, std::string &info) {
  if (WIFEXITED(wstatus)) {
    auto code = WEXITSTATUS(wstatus);
    info = util::format("exited with code %d", code);
    return code == 0;
  }

  if (WIFSIGNALED(wstatus)) {
    info = util::format("terminated by signal %d", WTERMSIG(wstatus));
    return false;
  }

  info = util::format("is dead with wstatus=%d", wstatus);
  return false;
}

int HookExec(const Hook &hook) {
  int execPid = fork();
  if (execPid < 0) {
    logErr() << "fork failed" << util::RetErrString(execPid);
    return -1;
  }

  if (execPid == 0) {
    util::str_vec argStrVec;
    argStrVec.push_back(hook.path);

    std::copy(hook.args->begin() + 1, hook.args->end(),
              std::back_inserter(argStrVec));

    util::Exec(argStrVec, hook.env.value_or(std::vector<std::string>{}));

    exit(0);
  }

  return waitpid(execPid, nullptr, 0);
}

int Container::NonePrivilegeProc(void *self) {
  // TODO(iceyer): use option
  auto *container = static_cast<Container *>(self);
  Linux linux;
  IDMap idMap;

  idMap.containerID = container->hostUid;
  idMap.hostID = container->hostUid;
  idMap.size = 1;
  linux.uidMappings.push_back(idMap);

  idMap.containerID = container->hostGid;
  idMap.hostID = container->hostGid;
  idMap.size = 1;
  linux.gidMappings.push_back(idMap);

  if (auto ret = ConfigUserNamespace(linux, 0); ret != 0) {
    return ret;
  }

  auto ret = mount("proc", "/proc", "proc", 0, nullptr);
  if (0 != ret) {
    logErr() << "mount proc failed" << util::RetErrString(ret);
    return -1;
  }

  if (container->runtime.hooks.has_value()) {
    for (auto const &preStart :
         container->runtime.hooks->prestart.value_or(std::vector<Hook>{})) {
      HookExec(preStart);
    }
    for (auto const &startContainer :
         container->runtime.hooks->startContainer.value_or(
             std::vector<Hook>{})) {
      HookExec(startContainer);
    }
  }

  if (!container->forkAndExecProcess(container->runtime.process)) {
    logErr() << "fork and exec failed";
    return -1;
  }

  return util::WaitAllUntil(container->pidMap.begin()->first);
}

void sigtermHandler(int /*unused*/) { ::exit(EXIT_FAILURE); }

int Container::EntryProc(void *self) {
  auto *container = static_cast<Container *>(self);
  if (auto ret = ConfigUserNamespace(container->runtime.linux, 0); ret != 0) {
    return ret;
  }

  // FIXME: change HOSTNAME will broken XAUTH
  auto new_hostname = container->runtime.hostname;
  //    if (sethostname(new_hostname.c_str(), strlen(new_hostname.c_str())) ==
  //    -1) {
  //        logErr() << "sethostname failed" << util::errnoString();
  //        return -1;
  //    }

  uint32_t flags = MS_REC | MS_SLAVE;
  int ret = mount(nullptr, "/", nullptr, flags, nullptr);
  if (0 != ret) {
    logErr() << "mount / failed" << util::RetErrString(ret);
    return -1;
  }

  container->MountContainerPath();

  if (container->useNewCgroupNs) {
    auto ret = ConfigCgroupV2(container->runtime.linux.cgroupsPath,
                              container->runtime.linux.resources, getpid());
    if (ret != 0) {
      logErr() << "config cgroupV2 failed";
      return -1;
    }
  }

  if (auto ret = container->PrepareDefaultDevices(); ret == -1) {
    logWan() << "prepare default devices failed";
  }

  if (auto ret = container->PivotRoot(); ret == -1) {
    logErr() << "pivotRoot failed";
    return -1;
  }

  if (auto ret = PrepareLinks(); ret == -1) {
    logWan() << "prepareLinks failed";
    return -1;
  }

  int nonePrivilegeProcFlag =
      SIGCHLD | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNS;

  int noPrivilegePid = util::PlatformClone(&Container::NonePrivilegeProc,
                                           nonePrivilegeProcFlag, self);
  if (noPrivilegePid < 0) {
    logErr() << "clone failed" << util::RetErrString(noPrivilegePid);
    return -1;
  }

  if (DropPermissions() != 0) {
    logWan() << "drop permissions failed";
  }

  // FIXME: parent may dead before this return.
  prctl(PR_SET_PDEATHSIG, SIGKILL);

  // FIXME(interactive bash): if need keep interactive shell

  signal(SIGTERM, sigtermHandler);
  return util::WaitAllUntil(noPrivilegePid);
}

Container::Container(std::filesystem::path bundle, std::string id,
                     Runtime runtime)
    : runtime(std::move(runtime)),
      bundle(std::move(bundle)),
      id(std::move(id)),
      hostRoot([this] {  // FIXME: redesign this class
        if (this->runtime.root.path.front() != '/') {
          return this->bundle / this->runtime.root.path;
        }
        return std::filesystem::path{this->runtime.root.path};
      }()),
      containerMounter(this->hostRoot) {}

int Container::DropPermissions() {
  __gid_t newgid[1] = {getgid()};
  auto newuid = getuid();
  auto olduid = geteuid();

  if (0 == olduid) {
    if (setgroups(1, newgid) == -1) {
      logWan() << "setgroups err:" << util::errnoString();
    }
  }

  if (seteuid(newuid) == -1) {
    logWan() << "seteuid err:" << util::errnoString();
  }

  return 0;
}

int Container::PrepareLinks() {
  if (auto ret = chdir("/"); ret == -1) {
    logErr() << "chdir err:" << util::RetErrString(ret);
    return -1;
  }

  const static std::unordered_map<std::string_view, std::string_view> linkMap =
      {{"/proc/kcore", "/dev/core"},
       {"/proc/self/fd", "/dev/fd"},
       {"/proc/self/fd/2", "/dev/stderr"},
       {"/proc/self/fd/0", "/dev/stdin"},
       {"/proc/self/fd/1", "/dev/stdout"}};

  // default link
  for (const auto &[from, to] : linkMap) {
    if (auto ret = symlink(from.data(), to.data()); ret == -1) {
      logErr() << "symlink" << from << "to" << to
               << "failed:" << util::RetErrString(ret);
      return -1;
    }
  }

  return 0;
}

int Container::PrepareDefaultDevices() {
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

  // TODO(iceyer): not work now
  for (auto const &dev : list) {
    Mount mount;
    mount.destination = dev.path;
    mount.type = "bind";
    mount.data = std::vector<std::string>{};
    mount.flags = MS_BIND;
    mount.fsType = Mount::Bind;
    mount.source = dev.path;

    if (!containerMounter.MountNode(mount)) {
      logErr() << "failed to mount default device" << dev.path;
      return -1;
    }
  }

  // FIXME(iceyer): /dev/console is set up if terminal is enabled in the config
  // by bind mounting the pseudoterminal slave to /dev/console.
  std::error_code ec;
  auto hostPtmx = hostRoot / "dev" / "ptmx";
  std::filesystem::path containerPtmx{"/dev/pts/ptmx"};
  std::filesystem::create_symlink(containerPtmx, hostPtmx, ec);
  if (ec) {
    logErr() << "create symlink from" << hostPtmx.string() << "to"
             << containerPtmx.string() << "error:" << ec.message();
    return -1;
  }

  return 0;
}

void Container::waitChildAndExec() {
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
  if (sfd == -1) logWan() << "signalfd";

  auto epfd = epoll_create(1);
  epoll_ctl_add(epfd, sfd);

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
          logWan() << util::format("Read unexpected signal [%d]\n",
                                   fdsi.ssi_signo);
        }
      } else {
        logWan() << "Unknown fd";
      }
    }
  }
}

bool Container::forkAndExecProcess(const Process &process, bool unblock) {
  // FIXME: parent may dead before this return.
  if (auto ret = prctl(PR_SET_PDEATHSIG, SIGKILL); ret == -1) {
    logErr() << "couldn't manipulate current process"
             << util::RetErrString(ret);
    return false;
  }

  int pid = fork();
  if (pid < 0) {
    logErr() << "fork failed" << util::RetErrString(pid);
    return false;
  }

  if (0 == pid) {
    if (unblock) {
      // FIXME: As we use signalfd, we have to block signal, but child created
      // by fork will inherit blocked signal set, so we have to unblock it. This
      // is just a workaround.
      sigset_t mask;
      sigfillset(&mask);
      if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
        logWan() << "sigprocmask unblock";
    }
    logDbg() << "process.args:" << process.args;

    if (auto ret = chdir(process.cwd.c_str()); ret != 0) {
      logErr() << "failed to chdir to" << process.cwd.c_str()
               << util::RetErrString(ret);
      return false;
    }

    for (const auto &env : process.env) {
      if (env.rfind("PATH=", 0) != 0) {
        continue;
      }

      if (auto ret = setenv("PATH", env.c_str() + strlen("PATH="), 1);
          ret == -1) {
        logWan() << "failed to set PATH" << util::RetErrString(ret);
      }
    }

    logInf() << "start exec process";
    if (auto ret = util::Exec(process.args, process.env); ret != 0) {
      logErr() << "exec failed" << util::RetErrString(ret);
      exit(ret);
    }
  } else {
    pidMap.insert(make_pair(pid, process.args[0]));
  }

  return true;
}

int Container::PivotRoot() const {
  containerMounter.finalizeMounts();

  int ret = -1;
  ret = chdir(hostRoot.c_str());
  if (ret != 0) {
    logErr() << "chdir error:" << util::RetErrString(ret);
    return -1;
  }

  int flag = MS_BIND | MS_REC;
  ret = mount(".", ".", "bind", flag, nullptr);
  if (0 != ret) {
    logErr() << "mount / failed" << util::RetErrString(ret);
    return -1;
  }

  const auto *llHostFilename = "ll-host";
  auto llHostInPath = std::string{"run/"} + llHostFilename;
  auto llHostOutPath = hostRoot / llHostInPath;
  ret = mkdir(llHostOutPath.c_str(), 0755);
  if (ret != 0) {
    logErr() << "mkdir" << llHostOutPath << "err:" << util::RetErrString(ret);
    return -1;
  }

  ret = syscall(SYS_pivot_root, hostRoot.c_str(), llHostOutPath.c_str());
  if (0 != ret) {
    logErr() << "SYS_pivot_root failed" << hostRoot << util::errnoString()
             << errno << ret;
    return -1;
  }

  ret = chdir("/");
  if (ret != 0) {
    logErr() << "chdir to root [before chroot]:" << util::RetErrString(ret);
    return -1;
  }

  ret = chroot(".");
  if (0 != ret) {
    logErr() << "chroot failed" << hostRoot << util::errnoString() << errno;
    return -1;
  }

  ret = chdir("/");
  if (ret != 0) {
    logErr() << "chdir to root [after chroot]:" << util::RetErrString(ret);
    return -1;
  }

  ret = umount2(llHostInPath.c_str(), MNT_DETACH);
  if (ret != 0) {
    logErr() << "umount2" << llHostInPath << "err:" << util::RetErrString(ret);
    return -1;
  }

  return 0;
}

int Container::MountContainerPath() {
  if (runtime.mounts.has_value()) {
    for (auto &mount : runtime.mounts.value()) {
      // complete source path
      if (!mount.source.empty() && mount.source.at(0) != '/') {
        mount.source = (bundle / mount.source).string();
      }
      logDbg() << "mount" << mount.source << "to" << mount.destination;
      if (!containerMounter.MountNode(mount)) {
        logWan() << "failed to Mount:" << mount.source << "to"
                 << mount.destination;
      }
    }
  };

  return 0;
}

int Container::Start() {
  hostUid = static_cast<int>(::geteuid());
  hostGid = static_cast<int>(::getegid());
  int flags = SIGCHLD | CLONE_NEWNS;

  for (auto const &n : runtime.linux.namespaces) {
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
        useNewCgroupNs = true;
        break;
      default:
        return -1;
    }
  }

  flags |= CLONE_NEWUSER;

  int entryPid = util::PlatformClone(EntryProc, flags, this);
  if (entryPid < 0) {
    logErr() << "clone failed" << util::RetErrString(entryPid);
    return -1;
  }

  // FIXME: maybe we need c.opt.child_need_wait?

  if (DropPermissions() != 0) {
    logWan() << "drop permissions failed";
  }

  // FIXME: parent may dead before this return.
  prctl(PR_SET_PDEATHSIG, SIGKILL);

  writeContainerJson(this->bundle, this->id, entryPid);

  // FIXME(interactive bash): if need keep interactive shell
  auto ret = util::WaitAllUntil(entryPid);

  auto dir = std::filesystem::path("/run") / "user" / std::to_string(getuid()) /
             "linglong" / "box";
  if (!std::filesystem::remove(dir / (this->id + ".json"))) {
    logErr() << "remove" << dir / (this->id + ".json") << "failed";
  }

  return ret;
}

Container::~Container() = default;

}  // namespace linglong
