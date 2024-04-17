/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "debug.h"

#include "util/logger.h"

#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong {

#define DUMP_DBG(func, line) (linglong::util::Logger(linglong::util::Logger::Debug, func, line))

void DumpIDMap()
{
    logDbg() << "DumpIDMap Start -----------";
    std::ifstream uidMap("/proc/self/uid_map");
    for (std::string line; getline(uidMap, line);) {
        logDbg() << "uid_map of pid:" << getpid() << line;
    }

    std::ifstream gidMap("/proc/self/gid_map");
    for (std::string line; getline(gidMap, line);) {
        logDbg() << "gid_map of pid:" << getpid() << line;
    }

    auto setgroupsPath = util::format("/proc/self/setgroups");
    std::ifstream setgroupsFileRead(setgroupsPath);
    std::string line;
    std::getline(setgroupsFileRead, line);
    logDbg() << "setgroups of pid:" << getpid() << line;
    logDbg() << "DumpIDMap end -----------";
}

void DumpUidGidGroup()
{
    logDbg() << "DumpUidGidGroup Start -----------";
    //    __uid_t uid = getuid(); // you can change this to be the uid that you want
    //
    //    struct passwd *pw = getpwuid(uid);
    //    if (pw == NULL) {
    //        perror("getpwuid error: ");
    //    }
    //
    //    int ngroups = 0;
    //
    //    // this call is just to get the correct ngroups
    //    getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ngroups);
    //    __gid_t groups[ngroups];
    //
    //    // here we actually get the groups
    //    getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
    //
    //    // example to print the groups name
    //    for (int i = 0; i < ngroups; i++) {
    //        struct group *gr = getgrgid(groups[i]);
    //        if (gr == NULL) {
    //            perror("getgrgid error: ");
    //        }
    //        printf("%s\n", gr->gr_name);
    //    }

    logDbg() << "getuid" << getuid() << "geteuid" << geteuid();
    logDbg() << "getgid" << getgid() << "getegid" << getegid();
    const int groupSize = getgroups(0, NULL);
    __gid_t list[groupSize + 1];
    getgroups(groupSize, list);

    std::string groupListStr;
    for (int i = 0; i < groupSize; ++i) {
        groupListStr += util::format("%d ", list[i]);
    }
    logDbg() << "getgroups size " << groupSize << ", list:" << groupListStr;
    logDbg() << "DumpUidGidGroup end -----------";
}

void DumpFilesystem(const std::string &path, const char *func, int line)
{
    if (nullptr == func) {
        func = const_cast<char *>(__FUNCTION__);
    }
    DUMP_DBG(func, line) << "DumpFilesystem begin -----------" << path;
    DIR *dir;
    if ((dir = opendir(path.c_str())) != NULL) {
        struct dirent *ent;
        /* print all the files and directories within directory */
        while ((ent = readdir(dir)) != NULL) {
            DUMP_DBG(func, line) << path + "/" + ent->d_name;
        }
        closedir(dir);
    } else {
        /* could not open directory */
        logErr() << linglong::util::errnoString() << errno;
        return;
    }
    DUMP_DBG(func, line) << "DumpFilesystem end -----------" << path;
}

void DumpFileInfo(const std::string &path)
{
    DumpFileInfo1(path, __FUNCTION__, __LINE__);
}

void DumpFileInfo1(const std::string &path, const char *func, int line)
{
    struct stat st
    {
    };

    auto ret = lstat(path.c_str(), &st);
    if (0 != ret) {
        DUMP_DBG(func, line) << path << util::RetErrString(ret);
    } else {
        DUMP_DBG(func, line) << path << st.st_uid << st.st_gid
                             << ((st.st_mode & S_IFMT) == S_IFDIR);
    }
}

} // namespace linglong
