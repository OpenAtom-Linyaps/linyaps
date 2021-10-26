/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ostree_repohelper.h"
#include "repohelper_factory.h"

namespace linglong {

/*
 * 创建仓库工具类
 *
 * @param type: 仓库类型
 *
 * @return RepoHelper: type对应的仓库类型
 */
RepoHelper *RepoHelperFactory::createRepoHelper(RepoType type)
{
    if (RepoType::OSTreeRepo == type) {
        return OstreeRepoHelper::get();
    }
    return nullptr;
}
} // namespace linglong
