/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "repo.h"

#include <QString>
#include <QDir>

#include "module/util/sysinfo.h"
#include "module/package/info.h"

#include "repo_client.h"
#include "ostree_repo.h"

namespace linglong {
namespace repo {

void registerAllMetaType()
{
    qJsonRegister<ParamStringMap>();
    qJsonRegister<linglong::repo::InfoResponse>();
    qJsonRegister<linglong::repo::UploadResponseData>();
    qJsonRegister<linglong::repo::AuthResponse>();
    qJsonRegister<linglong::repo::AuthResponseData>();
    qJsonRegister<linglong::repo::RevPair>();
    qJsonRegister<linglong::repo::UploadRequest>();
    qJsonRegister<linglong::repo::UploadTaskRequest>();
    qJsonRegister<linglong::repo::UploadTaskResponse>();
}

} // namespace repo
} // namespace linglong
