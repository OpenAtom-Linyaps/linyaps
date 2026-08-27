// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/package_manager/action.h"

#include <string>

namespace linglong::service {

class ExportBinaryAction : public Action
{
public:
    static std::shared_ptr<ExportBinaryAction> create(std::string appID,
                                                      std::string scriptName,
                                                      std::string commandName,
                                                      PackageManager &pm,
                                                      repo::OSTreeRepo &repo)
    {
        return std::shared_ptr<ExportBinaryAction>(new ExportBinaryAction(std::move(appID),
                                                                          std::move(scriptName),
                                                                          std::move(commandName),
                                                                          pm,
                                                                          repo));
    }

    ~ExportBinaryAction() override = default;

    utils::error::Result<void> prepare() override { return LINGLONG_OK; }

    utils::error::Result<void> doAction(PackageTask &task) override;

    std::string getTaskName() const override { return "ExportBinary"; }

private:
    ExportBinaryAction(std::string appID,
                       std::string scriptName,
                       std::string commandName,
                       PackageManager &pm,
                       repo::OSTreeRepo &repo)
        : Action(pm, repo, {})
        , appID(std::move(appID))
        , scriptName(std::move(scriptName))
        , commandName(std::move(commandName))
    {
    }

    std::string appID;
    std::string scriptName;
    std::string commandName;
};

} // namespace linglong::service
