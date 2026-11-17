// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/package/reference.h"

#include <mutex>
#include <string>

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
    virtual std::string getTaskName() const;

protected:
    void setTaskName(std::string name);
    void setTaskMessage(std::string message);
    std::string getTaskMessage() const;

    utils::error::Result<ActionOperation>
    getActionOperation(const api::types::v1::PackageInfoV2 &target, bool extraModuleOnly);

    PackageManager &pm;
    repo::OSTreeRepo &repo;
    api::types::v1::CommonOptions options;

private:
    mutable std::mutex taskTextMutex;
    std::string taskName;
    std::string taskMessage;
};

} // namespace linglong::service
