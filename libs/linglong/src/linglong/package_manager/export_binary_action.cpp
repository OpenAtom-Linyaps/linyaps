// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package_manager/export_binary_action.h"

#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

namespace linglong::service {

utils::error::Result<void> ExportBinaryAction::doAction(PackageTask &task)
{
    task.updateState(api::types::v1::State::Processing, "exporting binary " + scriptName);

    auto result = repo.exportAppBinary(appID, scriptName, commandName);
    if (!result) {
        LogE("failed to export binary {} for {}: {}", scriptName, appID, result.error());
        return result;
    }

    task.updateState(api::types::v1::State::Succeed,
                     fmt::format("binary {} exported for {}", scriptName, appID));
    return LINGLONG_OK;
}

} // namespace linglong::service
