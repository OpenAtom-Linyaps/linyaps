/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "host_mount.h"

#include "filesystem_driver.h"
#include "util/debug/debug.h"
#include "util/logger.h"

#include <linux/limits.h>

#include <utility>

#include <fcntl.h>
#include <sys/stat.h>

struct remountNode
{
    uint32_t flags{ 0U };
    uint32_t extraFlags{ 0U };
    int targetFd{ -1 };
    std::string targetPath;
    std::string data;
};

namespace linglong {

class HostMountPrivate
{
public:
    explicit HostMountPrivate() = default;

    int CreateDestinationPath(const util::fs::path &container_destination_path) const
    {
        return driver_->CreateDestinationPath(container_destination_path);
    }

    int MountNode(const struct Mount &m) const
    {
        int ret = -1;

        struct stat source_stat
        {
        };

        bool is_path = false;

        auto source = m.source;

        if (!m.source.empty() && m.source[0] == '/') {
            is_path = true;
            source = driver_->HostSource(util::fs::path(m.source)).string();
        }

        ret = lstat(source.c_str(), &source_stat);
        if (0 == ret) {
        } else {
            // source not exist
            if (m.fsType == Mount::Bind) {
                logErr() << "lstat" << source << "failed";
                return -1;
            }
        }

        auto dest_full_path = util::fs::path(m.destination);
        auto dest_parent_path = util::fs::path(dest_full_path).parent_path();
        auto host_dest_full_path = driver_->HostPath(dest_full_path);
        auto root = driver_->HostPath(util::fs::path("/"));
        int sourceFd{ -1 }; // FIXME: use local variable store fd temporarily, we should refactoring
                            // the whole MountNode in the future

        switch (source_stat.st_mode & S_IFMT) {
        case S_IFCHR: {
            driver_->CreateDestinationPath(dest_parent_path);
            host_dest_full_path.touch();
            break;
        }
        case S_IFSOCK: {
            driver_->CreateDestinationPath(dest_parent_path);
            // FIXME: can not mound dbus socket on rootless
            host_dest_full_path.touch();
            break;
        }
        case S_IFLNK: {
            driver_->CreateDestinationPath(dest_parent_path);
            if (m.extraFlags & OPTION_COPY_SYMLINK) {
                std::array<char, PATH_MAX + 1> buf{};
                auto len = readlink(source.c_str(), buf._M_elems, PATH_MAX);
                if (len == -1) {
                    logErr() << "readlink failed:" << source << strerror(errno);
                    return -1;
                }

                return host_dest_full_path.touch_symlink(std::string(buf.cbegin(), buf.cend()));
            }

            host_dest_full_path.touch();

            if (m.extraFlags & OPTION_NOSYMFOLLOW) {
                sourceFd = ::open(source.c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC);
                if (sourceFd < 0) {
                    logFal() << util::format("fail to open source(%s):", source.c_str())
                             << util::errnoString();
                }

                source = util::format("/proc/self/fd/%d", sourceFd);
                break;
            }

            source = util::fs::read_symlink(util::fs::path(source)).string();
            break;
        }
        case S_IFREG: {
            driver_->CreateDestinationPath(dest_parent_path);
            host_dest_full_path.touch();
            break;
        }
        case S_IFDIR:
            driver_->CreateDestinationPath(dest_full_path);
            break;
        default:
            driver_->CreateDestinationPath(dest_full_path);
            if (is_path) {
                logWan() << "unknown file type" << (source_stat.st_mode & S_IFMT) << source;
            }
            break;
        }

        auto data = util::str_vec_join(m.data, ',');
        auto real_data = data;
        auto real_flags = m.flags;

        switch (m.fsType) {
        case Mount::Bind: {
            // make sure m.flags always have MS_BIND
            real_flags |= MS_BIND;

            // When doing a bind mount, all flags expect MS_BIND and MS_REC are ignored by kernel.
            real_flags &= (MS_BIND | MS_REC);

            // When doing a bind mount, data and fstype are ignored by kernel. We should set them by
            // remounting.
            real_data = "";
            ret = util::fs::do_mount_with_fd(root.c_str(),
                                             source.c_str(),
                                             host_dest_full_path.string().c_str(),
                                             nullptr,
                                             real_flags & ~MS_RDONLY,
                                             nullptr);
            if (0 != ret) {
                break;
            }

            if (source == "/sys") {
                sysfs_is_binded = true;
            }

            if (data.empty() && (m.flags & ~(MS_BIND | MS_REC | MS_REMOUNT)) == 0) {
                // no need to be remounted
                break;
            }

            if (m.extraFlags & OPTION_NOSYMFOLLOW) {
                break; // FIXME: Refactoring the mounting process
            }

            real_flags = m.flags | MS_BIND | MS_REMOUNT | MS_RDONLY;
            auto newFd = ::open(host_dest_full_path.string().c_str(), O_PATH | O_CLOEXEC);
            if (newFd == -1) {
                logErr() << "failed to open" << host_dest_full_path.string();
                break;
            }
            // When doing a remount, source and fstype are ignored by kernel.
            remountList.emplace_back(remountNode{
              .flags = real_flags,
              .extraFlags = m.extraFlags,
              .targetFd = newFd,
              .targetPath = util::format("/proc/self/fd/%d", newFd),
              .data = data,
            });
            break;
        }
        case Mount::Proc:
        case Mount::Devpts:
        case Mount::Mqueue:
        case Mount::Tmpfs:
        case Mount::Sysfs: {
            ret = util::fs::do_mount_with_fd(root.c_str(),
                                             source.c_str(),
                                             host_dest_full_path.string().c_str(),
                                             m.type.c_str(),
                                             real_flags,
                                             real_data.c_str());
            if (ret < 0) {
                // refers:
                // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L178
                // https://github.com/containers/crun/blob/38e1b5e2a3e9567ff188258b435085e329aaba42/src/libcrun/linux.c#L768-L789
                if (m.fsType == Mount::Sysfs) {
                    real_flags = MS_BIND | MS_REC;
                    real_data = "";
                    ret = util::fs::do_mount_with_fd(root.c_str(),
                                                     "/sys",
                                                     host_dest_full_path.string().c_str(),
                                                     nullptr,
                                                     real_flags,
                                                     nullptr);
                    if (ret == 0) {
                        sysfs_is_binded = true;
                    }
                } else if (m.fsType == Mount::Mqueue) {
                    real_flags = MS_BIND | MS_REC;
                    real_data = "";
                    ret = util::fs::do_mount_with_fd(root.c_str(),
                                                     "/dev/mqueue",
                                                     host_dest_full_path.string().c_str(),
                                                     nullptr,
                                                     real_flags,
                                                     nullptr);
                }
            }
            break;
        }
        case Mount::Cgroup:
            ret = util::fs::do_mount_with_fd(root.c_str(),
                                             source.c_str(),
                                             host_dest_full_path.string().c_str(),
                                             m.type.c_str(),
                                             real_flags,
                                             real_data.c_str());
            // When sysfs is bind-mounted, It is ok to let cgroup mount failed.
            // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L281
            if (sysfs_is_binded) {
                ret = 0;
            }
            break;
        default:
            logErr() << "unsupported type" << m.type;
        }

        if (EXIT_SUCCESS != ret) {
            logErr() << "mount" << source << "to" << host_dest_full_path
                     << "failed:" << util::RetErrString(ret) << "\nmount args is:" << m.type
                     << real_flags << real_data;
            if (is_path) {
                logErr() << "source file type is: 0x" << std::oct << (source_stat.st_mode & S_IFMT);
                DUMP_FILE_INFO(source);
            }
            DUMP_FILE_INFO(host_dest_full_path.string());
        }

        if (sourceFd != -1) {
            ::close(sourceFd);
        }

        return ret;
    }

    void finalizeMounts()
    {
        for (const auto &node : remountList) {
            if (::mount("none", node.targetPath.c_str(), "", node.flags, node.data.c_str()) != 0) {
                logWan() << "failed to remount" << node.targetPath << strerror(errno);
            }

            if (::close(node.targetFd) == -1) {
                logWan() << "failed to close fd" << node.targetFd << strerror(errno);
            }
        }
    }

    mutable std::vector<remountNode> remountList;
    std::unique_ptr<FilesystemDriver> driver_;
    mutable bool sysfs_is_binded = false;
};

HostMount::HostMount()
    : dd_ptr(new HostMountPrivate())
{
}

int HostMount::MountNode(const struct Mount &m) const
{
    return dd_ptr->MountNode(m);
}

void HostMount::finalizeMounts() const
{
    dd_ptr->finalizeMounts();
}

int HostMount::Setup(FilesystemDriver *driver)
{
    if (nullptr == driver) {
        logWan() << this << dd_ptr->driver_.get();
        return 0;
    }

    dd_ptr->driver_ = std::unique_ptr<FilesystemDriver>(driver);
    return dd_ptr->driver_->Setup();
}

HostMount::~HostMount(){};

} // namespace linglong
