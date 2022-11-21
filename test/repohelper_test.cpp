/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "src/module/repo/ostree_repohelper.h"
#include "src/module/util/file.h"

//必须先使用ostree命令添加仓库,拷贝/usr/local/var/lib/flatpak/repo/flathub.trustedkeys.gpg  gpg 文件
// 否则无法下载flatpak仓库软件包
// 注意使用ostree添加的仓库和flatpak remote-add 结果不一样
// ostree 添加远端仓库命令
// ostree --repo=repo remote add --no-gpg-verify flathub https://flathub.org/repo/flathub.flatpakrepo
//添加玲珑测试仓库
// ostree --repo=repo remote add --no-gpg-verify repo http://10.20.54.2/linglong/pool/repo/
// flatpak命令添加远端仓库的命令如下：
// flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
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
//执行OstreeRepoHelperT02之后的用例需要将OstreeRepoHelperT01中生成的repo目录中的config文件改成上面的内容（相当于添加本地仓库操作）
// repoPath 目录要保证当前操作的用户可写，否则mkdir报错

TEST(RepoHelperT06, repoPullbyCmd)
{
    const QString repoPath = linglong::util::getLinglongRootPath();
    QString err = "";
    QVector<QString> repoList = {"repo"};

    QString matchRef = "org.deepin.calculator/1.0.0/x86_64";

    auto ret = OSTREE_REPO_HELPER->repoPullbyCmd(repoPath, repoList[0], matchRef, err);
    if (!ret) {
        qInfo() << err;
    }
}