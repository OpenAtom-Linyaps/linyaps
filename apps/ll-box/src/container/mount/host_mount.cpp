/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "host_mount.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "util/debug/debug.h"
#include "util/logger.h"
#include "util/oci_runtime.h"

std::string to_string(std::filesystem::file_type type) {
  switch (type) {
    case std::filesystem::file_type::none:
      return "none";
    case std::filesystem::file_type::not_found:
      return "not_found";
    case std::filesystem::file_type::regular:
      return "regular";
    case std::filesystem::file_type::directory:
      return "directory";
    case std::filesystem::file_type::symlink:
      return "symlink";
    case std::filesystem::file_type::block:
      return "block";
    case std::filesystem::file_type::character:
      return "character";
    case std::filesystem::file_type::fifo:
      return "fifo";
    case std::filesystem::file_type::socket:
      return "socket";
    case std::filesystem::file_type::unknown:
      return "unknown";
  }

  __builtin_unreachable();
}

bool do_mount_with_fd(const std::string &root, const std::string &source,
                      const std::string &destination,
                      const std::string &filesystemType,
                      unsigned long mountFlags, const void *data) noexcept {
  // https://github.com/opencontainers/runc/blob/0ca91f44f1664da834bc61115a849b56d22f595f/libcontainer/utils/utils.go#L112
  int fd = open(destination.c_str(), O_PATH | O_CLOEXEC);
  if (fd < 0) {
    logErr() << "fail to open destination " << destination
             << linglong::util::errnoString();
    return false;
  }

  linglong::util::defer closeFd{[fd] { ::close(fd); }};

  std::error_code ec;
  auto target = std::filesystem::path{"/proc/self/fd/"} / std::to_string(fd);
  auto realPath = std::filesystem::read_symlink(target, ec);
  if (ec) {
    logErr() << "failed to read symlink " << target.string()
             << " error:" << ec.message();
    return false;
  }

  auto realPathStr = realPath.string();
  if (realPathStr.rfind(root, 0) != 0) {
    logDbg() << "container root: " << root;
    logFal() << linglong::util::format(
        "possibly malicious path detected (%s vs %s) -- refusing to operate",
        target.c_str(), realPath.c_str());
  }

  int ret{-1};
  if (ret = ::mount(source.c_str(), target.c_str(), filesystemType.data(),
                    mountFlags, data);
      ret == 0) {
    return true;
  }

  logErr() << "mount" << source << "to" << realPathStr
           << "failed:" << linglong::util::RetErrString(ret)
           << "\nmount args: filesystemType [" << filesystemType
           << "], mountFlag [" << mountFlags << "], data:" << data;
  return false;
}

namespace linglong {

HostMount::HostMount(std::filesystem::path containerRoot)
    : containerRoot(std::move(containerRoot)) {}

std::filesystem::path HostMount::toHostDestination(
    const std::filesystem::path &containerDestination) noexcept {
  if (containerDestination.is_relative()) {
    return containerRoot / containerDestination;
  }

  return containerRoot / containerDestination.relative_path();
}

bool HostMount::ensureDirectoryExist(
    const std::filesystem::path &destination) noexcept {
  std::error_code ec;
  if (std::filesystem::exists(destination, ec)) {
    return true;
  }

  if (ec) {
    logErr() << "couldn't check existence of directory" << destination << ":"
             << ec.message();
    return false;
  }

  if (!std::filesystem::create_directories(destination, ec) && ec) {
    logErr() << "couldn't create directories" << destination.string() << ":"
             << ec.message();
    return false;
  }

  using perms = std::filesystem::perms;
  auto perm = perms::owner_all | perms::group_read | perms::group_exec |
              perms::others_read | perms::others_exec;  // 0755
  std::filesystem::permissions(destination, perm,
                               std::filesystem::perm_options::replace, ec);
  if (ec) {
    logErr() << "set " << destination.string()
             << "permission error:" << ec.message();
    return false;
  }

  return true;
}

bool HostMount::ensureFileExist(
    const std::filesystem::path &destination) noexcept {
  std::error_code ec;
  if (std::filesystem::exists(destination, ec)) {
    return true;
  }
  if (ec) {
    logErr() << "check" << destination.string()
             << "exist error:" << ec.message();
    return false;
  }

  if (!ensureDirectoryExist(destination.parent_path())) {
    logErr() << "failed ensure parent directory of " << destination.string();
    return false;
  }

  std::ofstream stream{destination};
  if (!stream.is_open()) {
    logErr() << "couldn't create file:" << destination.string();
    return false;
  }

  return true;
}

void HostMount::finalizeMounts() const {
  for (const auto &node : remountList) {
    auto targetPath =
        std::filesystem::path("/proc/self/fd") / std::to_string(node.targetFd);
    if (!remount(targetPath, node.flags, node.data)) {
      logWan() << "failed to remount:" << targetPath.string()
               << util::errnoString();
    }

    if (::close(node.targetFd) == -1) {
      logWan() << "failed to close fd:" << node.targetFd << " "
               << util::errnoString();
    }
  }
}

std::optional<bool> HostMount::isDummy(
    const std::string &filesystemType) noexcept {
  static std::unordered_map<std::string, bool> types;

  if (types.empty()) {
    const std::filesystem::path filesystemTypes = "/proc/filesystems";
    std::ifstream stream{filesystemTypes};
    if (!stream.is_open()) {
      std::cerr << "couldn't open " << filesystemTypes.string() << std::endl;
      return std::nullopt;
    }

    std::string line;
    while (std::getline(stream, line, '\n')) {
      auto it = line.cbegin();
      if (line.rfind("nodev", 0) == 0) {
        it += 6;  // skip 'nodev\t'
      } else {
        it += 1;  // skip '\t'
      }
      types.emplace(std::string(it, line.cend()), true);
    }
  }

  auto type = types.find(filesystemType);
  return type == types.cend() ? std::nullopt : std::make_optional(type->second);
}

bool HostMount::remount(const std::filesystem::path &target, uint32_t flags,
                        const std::string &data) {
  const char *data_ptr = data.c_str();
  if ((flags & (MS_REMOUNT | MS_RDONLY)) != 0U) {
    data_ptr = nullptr;
  }

  if (::mount("none", target.c_str(), "", flags, data_ptr) == 0) {
    return true;
  }

  // retry remount
  struct statfs sfs {};

  if (::statfs(target.c_str(), &sfs) < 0) {
    logErr() << "statfs file" << target << " " << util::errnoString();
    return false;
  }

  auto remount_flags =
      static_cast<uint32_t>(sfs.f_flags) & (MS_NOSUID | MS_NODEV | MS_NOEXEC);
  if ((flags | remount_flags) != flags) {
    if (::mount(nullptr, target.c_str(), nullptr, flags | remount_flags,
                data_ptr) == 0) {
      return true;
    }

    /* If it still fails and MS_RDONLY is present in the mount, try adding it.
     */
    if ((sfs.f_flags & MS_RDONLY) != 0) {
      remount_flags =
          sfs.f_flags & (MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY);
      if (::mount(nullptr, target.c_str(), nullptr, flags | remount_flags,
                  data_ptr) == -1) {
        logErr() << "failed to remount" << target << util::errnoString();
        return false;
      }
    }
  }

  return false;
}

bool HostMount::MountNode(const struct Mount &m) {
  std::error_code ec;

  auto source = std::filesystem::path{m.source};
  auto sourceStatus =
      std::filesystem::symlink_status(source, ec);  // same as lstat
  if (ec) {
    if (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory)) {
      logErr() << "check source status failed:" << ec.message();
      return false;
    }
  }

  auto destination = toHostDestination(m.destination);
  int sourceFd{-1};  // FIXME: use local variable store fd temporarily, we
                     // should refactoring the whole MountNode in the future
  util::defer closeFd([&sourceFd] {
    if (sourceFd != -1) {
      ::close(sourceFd);
    }
  });

  switch (sourceStatus.type()) {
    case std::filesystem::file_type::regular:
    case std::filesystem::file_type::block:
    case std::filesystem::file_type::fifo:
    case std::filesystem::file_type::character:
      [[fallthrough]];
    case std::filesystem::file_type::socket: {
      if (!ensureFileExist(destination)) {
        logErr() << "failed to ensure host destination exist.";
        return false;
      }
    } break;
    case std::filesystem::file_type::symlink: {
      if (!ensureDirectoryExist(destination.parent_path())) {
        logErr() << "failed to ensure the parent directory of host destination "
                    "exist.";
        return false;
      }

      if ((m.extensionFlags & Extension::COPY_SYMLINK) != 0U) {
        std::filesystem::copy_symlink(source, destination, ec);
        if (ec) {
          logErr() << "couldn't copy symlink from " << source.string() << " to "
                   << destination.string() << " error:" << ec.message();
        }

        return !ec;
      }

      auto nosymfollow = ((m.flags & LINGLONG_MS_NOSYMFOLLOW) != 0U);
      auto originalStatus = std::filesystem::status(source, ec);
      if (ec && !nosymfollow) {
        logErr() << "get the real path of symlink" << source.string()
                 << "error:" << ec.message();
        return false;
      }

      if (auto type = originalStatus.type();
          type == std::filesystem::file_type::directory && !nosymfollow) {
        if (!ensureDirectoryExist(destination)) {
          logErr() << "failed to ensure host directory exist.";
          return false;
        }
      } else {
        if (!ensureFileExist(destination)) {
          logErr() << "failed to ensure host file exist.";
          return false;
        }
      }

      if (nosymfollow) {
        sourceFd = ::open(source.c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC);
        if (sourceFd < 0) {
          logErr() << util::format("fail to open source(%s):", source.c_str())
                   << util::errnoString();
          return false;
        }

        source =
            std::filesystem::path{"/proc/self/fd"} / std::to_string(sourceFd);
        break;
      }

      source = std::filesystem::read_symlink(source, ec);
      if (ec) {
        logErr() << "couldn't read symlink from " << source.string()
                 << " error:" << ec.message();
        return false;
      }
    } break;
    case std::filesystem::file_type::directory: {
      if (!ensureDirectoryExist(destination)) {
        logErr() << "failed to ensure the directory of host destination exist.";
        return false;
      }
    } break;
    case std::filesystem::file_type::unknown: {
      logErr() << "source file type is unknown:" << source.string();
      return false;
    } break;
    case std::filesystem::file_type::not_found: {
      auto dummy = isDummy(m.type);
      if (dummy && !dummy.value()) {
        logErr() << "try to mount a not existing source" << source.string()
                 << "with filesystem" << m.type;
        return false;
      }

      if (!ensureDirectoryExist(destination)) {
        logErr() << "failed to ensure the directory of host destination exist.";
        return false;
      }
      source = m.type;
    } break;
    case std::filesystem::file_type::none: {
      logFal() << "file type never be none";
      __builtin_unreachable();
    }
  }

  auto data = util::str_vec_join(m.data, ',');
  auto real_data = data;
  auto real_flags = m.flags;
  const uint32_t all_propagations =
      (MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE);

  switch (m.fsType) {
    case Mount::Bind: {
      // make sure m.flags always have MS_BIND
      real_flags |= MS_BIND;

      // When doing a bind mount, all flags expect MS_BIND and MS_REC are
      // ignored by kernel.
      real_flags &= ((static_cast<uint32_t>(MS_BIND) | MS_REC) &
                     ~static_cast<uint32_t>(MS_RDONLY));

      // When doing a bind mount, data and fstype are ignored by kernel. We
      // should set them by remounting.
      real_data.clear();
      if (!do_mount_with_fd(containerRoot, source, destination, "", real_flags,
                            nullptr)) {
        break;
      }

      if (source.compare("/sys") == 0) {
        sysfs_is_binded = true;
      }

      if ((m.propagationFlags & all_propagations) != 0U) {
        auto rec = m.propagationFlags & MS_REC;
        auto propagation = m.propagationFlags & all_propagations;

        if (propagation != 0 &&
            !do_mount_with_fd(containerRoot, "", destination, "",
                              rec | propagation, nullptr)) {
          logErr() << "failed to set propagation for" << destination.string();
          break;
        }
      }

      if (data.empty() && (m.flags & ~(MS_BIND | MS_REC | MS_REMOUNT)) == 0) {
        // no need to be remounted
        return true;
      }

      if ((m.flags & LINGLONG_MS_NOSYMFOLLOW) != 0U) {
        return true;  // FIXME: Refactoring the mounting process
      }

      real_flags = m.flags | MS_BIND | MS_REMOUNT;
      if ((real_flags & MS_RDONLY) == 0) {
        if (remount(destination, real_flags, data)) {
          return true;
        }
        break;
      }

      auto newFd = ::open(destination.c_str(), O_PATH | O_CLOEXEC);
      if (newFd == -1) {
        logErr() << "failed to open " << destination.string()
                 << ", abort remount.";
        break;
      }

      // When doing a remount, source and fstype are ignored by kernel.
      remountList.emplace_back(remountNode{
          .flags = real_flags,
          .extensionFlags = m.extensionFlags,
          .targetFd = newFd,
          .data = data,
      });

      return true;
    }
    case Mount::Proc:
    case Mount::Devpts:
      [[fallthrough]];
    case Mount::Tmpfs: {
      if (do_mount_with_fd(containerRoot, source, destination, m.type,
                           real_flags, real_data.c_str())) {
        return true;
      }
    } break;
    case Mount::Mqueue: {
      if (do_mount_with_fd(containerRoot, source, destination, m.type,
                           real_flags, real_data.c_str())) {
        return true;
      }

      // refers:
      // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L178
      // https://github.com/containers/crun/blob/38e1b5e2a3e9567ff188258b435085e329aaba42/src/libcrun/linux.c#L768-L789
      real_flags = MS_BIND | MS_REC;
      if (do_mount_with_fd(containerRoot, "/dev/mqueue", destination, "",
                           real_flags, nullptr)) {
        return true;
      }
    } break;
    case Mount::Sysfs: {
      if (do_mount_with_fd(containerRoot, source, destination, m.type,
                           real_flags, real_data.c_str())) {
        return true;
      }

      // refers: Mqueue
      real_flags = MS_BIND | MS_REC;
      if (do_mount_with_fd(containerRoot, "/sys", destination, "", real_flags,
                           nullptr)) {
        sysfs_is_binded = true;
        return true;
      }
    } break;
    case Mount::Cgroup: {
      // When sysfs is bind-mounted, It is ok to let cgroup mount failed.
      // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L281
      if (do_mount_with_fd(containerRoot, source, destination, m.type,
                           real_flags, real_data.c_str()) ||
          sysfs_is_binded) {
        return true;
      }
    } break;
    default:
      logErr() << "unsupported type: " << m.type;
  }

  if (auto str = source.string(); !str.empty() && str.at(0) == '/') {
    logErr() << "source file type is: " << to_string(sourceStatus.type())
             << ", permission:" << std::oct
             << static_cast<int>(sourceStatus.permissions());
    linglong::DumpFileInfo(source);
  }

  linglong::DumpFileInfo(destination);
  return false;
}

}  // namespace linglong
