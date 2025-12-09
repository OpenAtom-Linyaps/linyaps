// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/package/reference.h"

namespace linglong::repo {
class OSTreeRepo;
}

namespace linglong::service {

class PackageManager;
class PackageTask;

struct ActionOperation
{
    enum Policy {
        Upgrade,
        Install,
        Remove,
        Overwrite,
        Downgrade,
    } operation;

    std::string kind;
    std::optional<package::Reference> oldRef;
    std::optional<package::ReferenceWithRepo> newRef;
};

// Action is used to run some operations
// prepare and doAction may be run in different context
class Action
{
public:
    Action(PackageManager &pm, repo::OSTreeRepo &repo, api::types::v1::CommonOptions options)
        : pm(pm)
        , repo(repo)
        , options(options)
    {
    }

    virtual ~Action() = default;

    virtual utils::error::Result<void> prepare() = 0;
    virtual utils::error::Result<void> doAction(PackageTask &task) = 0;
    virtual std::string getTaskName() const = 0;

protected:
    utils::error::Result<ActionOperation>
    getActionOperation(const api::types::v1::PackageInfoV2 &target, bool extraModuleOnly);

    PackageManager &pm;
    repo::OSTreeRepo &repo;
    api::types::v1::CommonOptions options;
};

} // namespace linglong::service
