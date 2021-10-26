/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>

namespace linglong {

class RepoHelper
{
public:
    RepoHelper() {}

    virtual ~RepoHelper() {}

    /*
     * 查询软件包在仓库中对应的索引信息ref
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param remoteName: 远端仓库名称
     * @param pkgName: 软件包包名
     * @param arch: 架构名
     * @param matchRef: 软件包对应的索引信息ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    virtual bool queryMatchRefs(const QString &repoPath, const QString &remoteName, const QString &pkgName, const QString &arch, QString &matchRef, QString &err) = 0;

    /*
     * 软件包数据从远端仓库pull到本地仓库
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param remoteName: 远端仓库名称
     * @param pkgName: 软件包包名
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    virtual bool repoPull(const QString &repoPath, const QString &remoteName, const QString &pkgName, QString &err) = 0;

    /*
     * 通过ostree命令将软件包数据从远端仓库pull到本地
     *
     * @param destPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    virtual bool repoPullbyCmd(const QString &destPath, const QString &remoteName, const QString &ref, QString &err) = 0;
};
} // namespace linglong