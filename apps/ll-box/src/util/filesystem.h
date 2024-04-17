/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_
#define LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_

#include "common.h"

#include <fstream>
#include <ostream>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace linglong {
namespace util {
namespace fs {

bool exists(const std::string &s);

class path : public std::basic_string<char>
{
public:
    explicit path(const std::string &s)
        : p(util::str_spilt(s, "/"))
    {
    }

    path &operator=(const std::string &s)
    {
        p = util::str_spilt(s, "/");
        return *this;
    }

    path &operator=(const path &p1) = default;

    bool operator==(const path &s) const { return this->p == s.p; }

    bool operator!=(const path &s) const { return this->p != s.p; }

    path operator/(const path &p1) const
    {
        auto np = *this;
        std::copy(p1.p.begin(), p1.p.end(), back_inserter(np.p));
        return np;
    }

    path operator/(const std::string &str) const { return operator/(path(str)); }

    path parent_path() const
    {
        path pn(*this);
        pn.p.pop_back();
        return pn;
    }

    std::string string() const { return "/" + str_vec_join(p, '/'); }

    str_vec components() const { return p; }

    // call to this function will block until `path` exists (rerturn 0) or timeout (return -1)
    // default timeout is 1 second
    int wait_until_exsit(int utimeout = 1000)
    {
        int time = 0;
        while (true) {
            if (access(this->string().c_str(), F_OK) == 0) {
                return 0;
            } else if (++time > utimeout) {
                return -1;
            } else {
                usleep(10);
            }
        }
        return 0;
    }

    void touch()
    {
        if (!exists(this->string())) {
            std::ofstream(this->string());
        }
    }

private:
    friend std::ostream &operator<<(std::ostream &cout, path obj);
    std::vector<std::string> p;
};

inline std::ostream &operator<<(std::ostream &cout, path obj)
{
    for (auto const &s : obj.p) {
        cout << "/" << s;
    }
    return cout;
}

bool create_directories(const path &p, __mode_t mode);

enum file_type {
    status_error,
    file_not_found,
    regular_file,
    directory_file,
    symlink_file,
    block_file,
    character_file,
    fifo_file,
    socket_file,
    reparse_file,
    type_unknown
};

enum perms {
    no_perms,
    owner_read,
    owner_write,
    owner_exe,
    owner_all,
    group_read,
    group_write,
    group_exe,
    group_all,
    others_read,
    others_write,
    others_exe,
    others_all,
    all_all,
    set_uid_on_exe,
    set_gid_on_exe,
    sticky_bit,
    perms_mask,
    perms_not_known,
    add_perms,
    remove_perms,
    symlink_perms
};

class file_status
{
public:
    // constructors
    file_status() noexcept;
    explicit file_status(file_type ft, perms p = perms_not_known) noexcept;

    // compiler generated
    file_status(const file_status &) noexcept;
    file_status &operator=(const file_status &) noexcept;
    ~file_status() noexcept;

    // observers
    file_type type() const noexcept;
    perms permissions() const noexcept;

private:
    file_type ft;
    perms p;
};

bool is_dir(const std::string &s);

file_status status(const path &p, std::error_code &ec);

path read_symlink(const path &p);

// This function do_mount_with_fd do mount in a secure way by check the target we are going to mount
// is within container rootfs or not before actually call mount. refer to
// https://github.com/opencontainers/runc/commit/0ca91f44f1664da834bc61115a849b56d22f595f
// NOTE: we should never directly do mount exists in oci json without this check.
int do_mount_with_fd(const char *root,
                     const char *__special_file,
                     const char *__dir,
                     const char *__fstype,
                     unsigned long int __rwflag,
                     const void *__data) __THROW;

} // namespace fs
} // namespace util
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_ */
