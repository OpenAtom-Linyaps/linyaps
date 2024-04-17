/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "filesystem.h"

#include "logger.h"

#include <sys/mount.h>

#include <climits>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace linglong {
namespace util {
namespace fs {

using namespace std;

bool create_directory(const path &p, __mode_t mode)
{
    return mkdir(p.string().c_str(), mode);
}

bool create_directories(const path &p, __mode_t mode)
{
    std::string fullPath;
    for (const auto &e : p.components()) {
        fullPath += "/" + e;
        if (is_dir(fullPath)) {
            continue;
        }

        auto ret = mkdir(fullPath.c_str(), mode);
        if (0 != ret) {
            logErr() << util::RetErrString(ret) << fullPath << mode;
            return false;
        }
    }
    return true;
}

bool is_dir(const std::string &s)
{
    struct stat st
    {
    };

    if (0 != lstat(s.c_str(), &st)) {
        return false;
    }

    switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
        return true;
    default:
        return false;
    }
}

bool exists(const std::string &s)
{
    struct stat st
    {
    };

    if (0 != lstat(s.c_str(), &st)) {
        return false;
    }
    return true;
}

path read_symlink(const path &p)
{
    char *buf = realpath(p.string().c_str(), nullptr);
    if (buf) {
        auto ret = path(string(buf));
        free(buf);
        return ret;
    } else {
        return p;
    }
}

file_status status(const path &p, std::error_code &ec)
{
    file_type ft;
    perms perm = no_perms;

    struct stat st
    {
    };

    if (0 != lstat(p.string().c_str(), &st)) {
        if (errno == ENOENT) {
            ft = file_not_found;
        } else {
            ft = status_error;
        }
        return file_status(ft, perm);
    }

    // FIXME: perms
    // https://www.boost.org/doc/libs/1_75_0/libs/filesystem/doc/reference.html#file_status
    //    int st_perm = st.st_mode & 0xFFFF;

    //    switch (st_perm) {
    //    case S_IRUSR:
    //        perm = owner_read;
    //        break;
    //    case S_IWUSR:
    //    case S_IXUSR:
    //    case S_IRWXU:
    //    case S_IRGRP:
    //    }

    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        ft = regular_file;
        break;
    case S_IFDIR:
        ft = directory_file;
        break;
    case S_IFLNK:
        ft = symlink_file;
        break;
    case S_IFBLK:
        ft = block_file;
        break;
    case S_IFCHR:
        ft = character_file;
        break;
    case S_IFIFO:
        ft = fifo_file;
        break;
    case S_IFSOCK:
        ft = socket_file;
        break;
    default:
        ft = type_unknown;
        break;
    }

    return file_status(ft, perm);
}

file_status::file_status() noexcept { }

file_status::file_status(file_type ft, perms perms) noexcept
    : ft(ft)
    , p(perms)
{
}

file_status::file_status(const file_status &fs) noexcept
{
    ft = fs.ft;
    p = fs.p;
}

file_status &file_status::operator=(const file_status &fs) noexcept
{
    ft = fs.ft;
    p = fs.p;
    return *this;
}

file_status::~file_status() noexcept = default;

file_type file_status::type() const noexcept
{
    return ft;
}

perms file_status::permissions() const noexcept
{
    return p;
}

int do_mount_with_fd(const char *root,
                     const char *__special_file,
                     const char *__dir,
                     const char *__fstype,
                     unsigned long int __rwflag,
                     const void *__data) __THROW
{
    // https://github.com/opencontainers/runc/blob/0ca91f44f1664da834bc61115a849b56d22f595f/libcontainer/utils/utils.go#L112

    int fd = open(__dir, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        logFal() << util::format("fail to open target(%s):", __dir) << errnoString();
    }

    // Refer to `man readlink`, readlink dose not append '\0' to the end of conent it read from
    // path, so we have to add an extra char to buffer to ensure '\0' always exists.
    char *buf = (char *)malloc(sizeof(char) * PATH_MAX + 1);
    if (buf == nullptr) {
        logFal() << "fail to alloc memery:" << errnoString();
    }

    memset(buf, 0, PATH_MAX + 1);

    auto target = util::format("/proc/self/fd/%d", fd);
    int len = readlink(target.c_str(), buf, PATH_MAX);
    if (len == -1) {
        logFal() << util::format("fail to readlink from proc fd (%s):", target.c_str())
                 << errnoString();
    }

    string realpath(buf);
    free(buf);
    if (realpath.rfind(root, 0) != 0) {
        logDbg() << util::format("container root=\"%s\"", root);
        logFal() << util::format(
          "possibly malicious path detected (%s vs %s) -- refusing to operate",
          target.c_str(),
          realpath.c_str());
    }

    auto ret = ::mount(__special_file, target.c_str(), __fstype, __rwflag, __data);
    auto olderrno = errno;

    close(fd);

    errno = olderrno;
    return ret;
}

} // namespace fs
} // namespace util
} // namespace linglong
