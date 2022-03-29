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

#include "repohelper.h"
#include "module/util/singleton.h"

namespace linglong {

enum RepoType {
    OSTreeRepo = 0, //linglong ostree仓库
    LinglongRepo // uos自研
};

class RepoHelperFactory : public linglong::util::Singleton<RepoHelperFactory>
{
public:
    /*
     * 创建仓库工具类
     *
     * @param type: 仓库类型
     *
     * @return RepoHelper: type对应的仓库类型
     */
    RepoHelper *createRepoHelper(RepoType type);
};
} // namespace linglong

#define G_REPOHELPER linglong::RepoHelperFactory::instance()->createRepoHelper(linglong::RepoType::OSTreeRepo)