// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "action.h"

#include "linglong/repo/ostree_repo.h"
#include "linglong/repo/repo_cache.h"

namespace linglong::service {

utils::error::Result<ActionOperation>
Action::getActionOperation(const api::types::v1::PackageInfoV2 &target, bool extraModuleOnly)
{
    LINGLONG_TRACE("get action operation");

    ActionOperation action;
    auto targetRef = package::Reference::fromPackageInfo(target);
    if (!targetRef) {
        return LINGLONG_ERR(targetRef);
    }

    if (extraModuleOnly) {
        action.operation = ActionOperation::Install;
    } else {
        repo::repoCacheQuery query;
        query.channel = target.channel;
        query.id = target.id;
        query.deleted = false;
        auto localRefs = repo.listLocalBy(query);
        if (!localRefs) {
            return LINGLONG_ERR(localRefs);
        }

        if (!localRefs->empty()) {
            // localRefs is sort by version
            auto oldRef = package::Reference::fromPackageInfo(localRefs->front().info);
            if (!oldRef) {
                return LINGLONG_ERR(oldRef);
            }
            if (targetRef->version > oldRef->version) {
                action.operation = ActionOperation::Upgrade;
            } else if (targetRef->version < oldRef->version) {
                action.operation = ActionOperation::Downgrade;
            } else {
                action.operation = ActionOperation::Overwrite;
            }

            // see if the same version is installed
            for (auto it = localRefs->begin() + 1; it != localRefs->end(); ++it) {
                auto ref = package::Reference::fromPackageInfo(it->info);
                if (!ref) {
                    return LINGLONG_ERR(ref);
                }

                if (ref->version == targetRef->version) {
                    action.operation = ActionOperation::Overwrite;
                    oldRef = std::move(ref);
                    break;
                }
            }

            // multiple version can be installed for non-app package
            if (action.operation != ActionOperation::Overwrite && target.kind != "app") {
                action.operation = ActionOperation::Install;
            }

            if (action.operation != ActionOperation::Install) {
                action.oldRef = std::move(oldRef).value();
            }
        } else {
            action.operation = ActionOperation::Install;
        }
    }

    action.kind = target.kind;
    action.newRef = package::ReferenceWithRepo{
        .reference = std::move(targetRef).value(),
    };

    return action;
}

} // namespace linglong::service
