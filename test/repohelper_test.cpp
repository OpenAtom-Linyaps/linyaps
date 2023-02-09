/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "src/module/repo/ostree_repohelper.h"
#include "src/module/util/file.h"

TEST(RepoHelperT06, repoPullbyCmd)
{
    const QString repoPath = linglong::util::getLinglongRootPath();
    QString err = "";
    QVector<QString> repoList = { "repo" };

    QString matchRef = "org.deepin.calculator/1.0.0/x86_64";

    auto ret = OSTREE_REPO_HELPER->repoPullbyCmd(repoPath, repoList[0], matchRef, err);
    if (!ret) {
        qInfo() << err;
    }
}