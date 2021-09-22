/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:
 *
 * Maintainer:
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

#include <gtest/gtest.h>
#include "module/repo/RepoHelper.h"

TEST(RepoHelperT01, ensureRepoEnv)
{
    char path[512] = {'\0'};
    getwd(path);
    const QString repoPath = path;
    QString err = "";
    linglong::RepoHelper repo;
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
    } else {
        std::cout << repoPath.toStdString();
    }
    std::cout.flush();
    EXPECT_EQ(ret, true);
}

//必须先使用ostree命令添加仓库,拷贝/usr/local/var/lib/flatpak/repo/flathub.trustedkeys.gpg  gpg 文件
// 否则无法下载flatpak仓库软件包
// 注意使用ostree添加的仓库和flatpak remote-add 结果不一样
//ostree 添加远端仓库命令
//ostree --repo=repo remote add --no-gpg-verify flathub https://flathub.org/repo/flathub.flatpakrepo
// flatpak命令添加远端仓库的命令如下：
//flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
// 上述命令执行完成后 会在/usr/local/var/lib/flatpak 目录生成一个 repo 目录 repo目录的config文件 就是仓库配置文件。
// [core]
// repo_version=1
// mode=bare-user-only

// [remote "flathub"]
// gpg-verify=true
// gpg-verify-summary=true
// url=https://dl.flathub.org/repo/
// xa.title=Flathub
// xa.title-is-set=true
//执行RepoHelperT02之后的用例需要将RepoHelperT01中生成的repo目录中的config文件改成上面的内容（相当于添加本地仓库操作）
// repoPath 目录要保证当前操作的用户可写，否则mkdir报错,repoPath中的xxxx可替换为用户名
#if 0
TEST(RepoHelperT02, getRemoteRepoList)
{
    const QString repoPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest";
    QString err = "";
    linglong::RepoHelper repo;
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
    EXPECT_EQ(ret, true);
    QVector<QString> qvec;
    ret = repo.getRemoteRepoList(repoPath, qvec, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = qvec.begin(); iter != qvec.end(); ++iter) {
            std::cout << (*iter).toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);
}

TEST(RepoHelperT03, getRemoteRefs)
{
    const QString repoPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest";
    QString err = "";
    linglong::RepoHelper repo;
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
    EXPECT_EQ(ret, true);
    QVector<QString> qvec;
    ret = repo.getRemoteRepoList(repoPath, qvec, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = qvec.begin(); iter != qvec.end(); ++iter) {
            std::cout << (*iter).toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QMap<QString, QString> outRefs;
    ret = repo.getRemoteRefs(repoPath, qvec[0], outRefs, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = outRefs.begin(); iter != outRefs.end(); ++iter) {
            std::cout << "ref:" << iter.key().toStdString() << ", commit value:" << iter.value().toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);
}

TEST(RepoHelperT04, resolveMatchRefs)
{
    const QString repoPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest";
    QString err = "";
    linglong::RepoHelper repo;
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
    EXPECT_EQ(ret, true);
    QVector<QString> qrepoList;
    ret = repo.getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = qrepoList.begin(); iter != qrepoList.end(); ++iter) {
            std::cout << (*iter).toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QMap<QString, QString> outRefs;
    ret = repo.getRemoteRefs(repoPath, qrepoList[0], outRefs, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = outRefs.begin(); iter != outRefs.end(); ++iter) {
            std::cout << "ref:" << iter.key().toStdString() << ", commit value:" << iter.value().toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QString matchRef = "";
    QString pkgName = "us.zoom.Zoom";
    QString arch = "x86_64";
    ret = repo.resolveMatchRefs(repoPath, qrepoList[0], pkgName, arch, matchRef, err);
    if (!ret) {
        std::cout << err.toStdString();
    } else {
        std::cout << matchRef.toStdString();
    }
    std::cout.flush();
    EXPECT_EQ(ret, true);
}

TEST(RepoHelperT05, repoPull)
{
    const QString repoPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest";
    QString err = "";
    linglong::RepoHelper repo;
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
    EXPECT_EQ(ret, true);
    QVector<QString> qrepoList;
    ret = repo.getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = qrepoList.begin(); iter != qrepoList.end(); ++iter) {
            std::cout << (*iter).toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QMap<QString, QString> outRefs;
    ret = repo.getRemoteRefs(repoPath, qrepoList[0], outRefs, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = outRefs.begin(); iter != outRefs.end(); ++iter) {
            std::cout << "ref:" << iter.key().toStdString() << ", commit value:" << iter.value().toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QString matchRef = "";
    QString pkgName = "us.zoom.Zoom";
    QString arch = "x86_64";
    ret = repo.resolveMatchRefs(repoPath, qrepoList[0], pkgName, arch, matchRef, err);
    if (!ret) {
        std::cout << err.toStdString();
    } else {
        std::cout << matchRef.toStdString();
    }
    std::cout.flush();
    EXPECT_EQ(ret, true);

    ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
    EXPECT_EQ(ret, true);
}

TEST(RepoHelperT06, checkOutAppData)
{
    const QString repoPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest";
    // 目录如果不存在 会自动创建目录
    const QString dstPath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/repotest/AppData";
    QString err = "";
    QVector<QString> qrepoList;
    linglong::RepoHelper repo;

    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }

    ret = repo.getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    } else {
        for (auto iter = qrepoList.begin(); iter != qrepoList.end(); ++iter) {
            std::cout << (*iter).toStdString() << endl;
            std::cout.flush();
        }
    }
    EXPECT_EQ(ret, true);

    QString matchRef = "";
    QString pkgName = "us.zoom.Zoom";
    QString arch = "x86_64";
    ret = repo.resolveMatchRefs(repoPath, qrepoList[0], pkgName, arch, matchRef, err);
    if (!ret) {
        std::cout << err.toStdString();
    } else {
        std::cout << matchRef.toStdString();
    }
    std::cout.flush();
    EXPECT_EQ(ret, true);

    ret = repo.checkOutAppData(repoPath, qrepoList[0], matchRef, dstPath, err);
    if (!ret) {
        std::cout << err.toStdString();
        std::cout.flush();
    }
}
#endif