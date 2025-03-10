/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "filesystem_driver.h"

#include "util/logger.h"
#include "util/platform.h"

#include <cerrno>
#include <utility>

#include <sys/wait.h>

namespace linglong {

FilesystemDriver::~FilesystemDriver() = default;

OverlayfsFuseFilesystemDriver::OverlayfsFuseFilesystemDriver(util::str_vec lower_dirs,
                                                             std::string upper_dir,
                                                             std::string work_dir,
                                                             std::string mount_point)
    : lower_dirs_(std::move(lower_dirs))
    , upper_dir_(std::move(upper_dir))
    , work_dir_(std::move(work_dir))
    , mount_point_(std::move(mount_point))
{
}

util::fs::path OverlayfsFuseFilesystemDriver::HostPath(const util::fs::path &dest_full_path) const
{
    return HostSource(util::fs::path(mount_point_) / dest_full_path);
}

int OverlayfsFuseFilesystemDriver::CreateDestinationPath(
  const util::fs::path &container_destination_path)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path =
      HostSource(util::fs::path(mount_point_) / container_destination_path);
    if (!util::fs::create_directories(host_destination_path, dest_mode)) {
        logErr() << "create_directories" << host_destination_path << util::errnoString();
        return -1;
    }

    return 0;
}

int OverlayfsFuseFilesystemDriver::Setup()
{
    // fork and exec fuse
    int pid = fork();
    if (0 == pid) {
        util::fs::create_directories(util::fs::path(work_dir_), 0755);
        util::fs::create_directories(util::fs::path(upper_dir_), 0755);
        util::fs::create_directories(util::fs::path(mount_point_), 0755);
        // fuse-overlayfs -o lowerdir=/ -o upperdir=./upper -o workdir=./work overlaydir
        util::str_vec args;
        args.push_back("/usr/bin/fuse-overlayfs");
        args.push_back("-o");
        args.push_back("lowerdir=" + util::str_vec_join(lower_dirs_, ':'));
        args.push_back("-o");
        args.push_back("upperdir=" + upper_dir_);
        args.push_back("-o");
        args.push_back("workdir=" + work_dir_);
        args.push_back(mount_point_);

        logErr() << util::Exec(args, {});
        logErr() << util::errnoString();

        exit(0);
    }
    util::Wait(pid);
    return 0;
}

util::fs::path OverlayfsFuseFilesystemDriver::HostSource(const util::fs::path &dest_full_path) const
{
    return dest_full_path;
}

util::fs::path NativeFilesystemDriver::HostPath(const util::fs::path &dest_full_path) const
{
    return util::fs::path(root_path_) / dest_full_path;
}

util::fs::path NativeFilesystemDriver::HostSource(const util::fs::path &dest_full_path) const
{
    return dest_full_path;
}

int NativeFilesystemDriver::CreateDestinationPath(const util::fs::path &container_destination_path)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = util::fs::path(root_path_) / container_destination_path;

    if (!util::fs::create_directories(host_destination_path, dest_mode)) {
        logErr() << "failed to create" << host_destination_path;
        return -1;
    }

    return 0;
}

NativeFilesystemDriver::NativeFilesystemDriver(std::string root_path)
    : root_path_(std::move(root_path))
{
}

NativeFilesystemDriver::~NativeFilesystemDriver() { }

FuseProxyFilesystemDriver::FuseProxyFilesystemDriver(util::str_vec mounts, std::string mount_point)
    : mounts_(mounts)
    , mount_point_(mount_point)
{
}

util::fs::path FuseProxyFilesystemDriver::HostPath(const util::fs::path &dest_full_path) const
{
    return HostSource(util::fs::path(mount_point_) / dest_full_path);
}

util::fs::path FuseProxyFilesystemDriver::HostSource(const util::fs::path &dest_full_path) const
{
    return dest_full_path;
}

int FuseProxyFilesystemDriver::CreateDestinationPath(
  const util::fs::path &container_destination_path)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path =
      HostSource(util::fs::path(mount_point_) / container_destination_path);
    if (!util::fs::create_directories(host_destination_path, dest_mode)) {
        logErr() << "create_directories" << host_destination_path << util::errnoString();
        return -1;
    }

    return 0;
}

int FuseProxyFilesystemDriver::Setup()
{
    int pipe_ends[2];
    if (0 != pipe(pipe_ends)) {
        logErr() << "pipe failed:" << util::errnoString();
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        logErr() << "fork failed:" << util::errnoString();
        return -1;
    }

    if (0 == pid) {
        close(pipe_ends[1]);
        if (-1 == dup2(pipe_ends[0], 112)) {
            logErr() << "dup2 failed:" << util::errnoString();
            return -1;
        }
        close(pipe_ends[0]);

        util::fs::create_directories(util::fs::path(mount_point_), 0755);
        util::fs::create_directories(util::fs::path(mount_point_ + "/.root"), 0755);

        util::str_vec args;
        args.push_back("/usr/bin/ll-fuse-proxy");
        args.push_back("112");
        args.push_back(mount_point_);

        logErr() << util::Exec(args, {});

        exit(0);
    } else {
        close(pipe_ends[0]);
        std::string root_mount = mount_point_ + "/.root:/\n";
        write(pipe_ends[1], root_mount.c_str(), root_mount.size()); // FIXME: handle write error
        for (auto const &m : mounts_) {
            write(pipe_ends[1], m.c_str(), m.size());
        }
        close(pipe_ends[1]);
        return 0;
    }
}

} // namespace linglong
