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

#include <sys/stat.h>
#include <string>
#include <climits>
#include <unistd.h>

#include "filesystem.h"

namespace util {
namespace fs {

using namespace std;

bool create_directory(const path &p, int mode)
{
    return mkdir(p.string().c_str(), mode);
}

bool create_directories(const path &p, int mode)
{
    std::string fullPath;
    for (const auto &e : p.components()) {
        fullPath += "/" + e;
        mkdir(fullPath.c_str(), mode);
    }
    return true;
}

bool is_dir(const std::string &s)
{
    struct stat st {
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
    struct stat st {
    };

    if (0 != lstat(s.c_str(), &st)) {
        return false;
    }
    return true;
}

path read_symlink(const path &p)
{
    char buf[PATH_MAX];
    if (readlink(p.string().c_str(), buf, sizeof(buf)) < 0) {
        return p;
    } else {
        return path(string(buf));
    }
}

file_status status(const path &p, std::error_code &ec)
{
    file_type ft;
    perms perm;

    struct stat st {
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
    int st_perm = st.st_mode & 0xFFFF;

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
    case S_IFSOCK:
        break;
    default:
        ft = type_unknown;
        break;
    }

    return file_status(ft, perm);
}

file_status::file_status() noexcept
{
}

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

file_status::~file_status() noexcept
{
}

file_type file_status::type() const noexcept
{
    return ft;
}

perms file_status::permissions() const noexcept
{
    return p;
}

} // namespace fs
} // namespace util
