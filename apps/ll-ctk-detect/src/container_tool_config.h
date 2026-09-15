// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace linglong::ctk::detect {

struct ContainerToolSpec
{
    std::string deviceId;
    std::vector<std::string> packages;
};

class ContainerToolConfigManager final
{
public:
    ContainerToolConfigManager(std::filesystem::path systemConfigPath,
                               std::filesystem::path userConfigPath);

    utils::error::Result<void> load();

    std::vector<ContainerToolSpec> tools() const { return tools_; }

    bool isNeverRemind(const std::string &deviceId) const;
    void setNeverRemind(const std::string &deviceId, bool neverRemind);
    utils::error::Result<void> saveUserConfig() const;

    const std::filesystem::path &userConfigPath() const { return userConfigPath_; }

private:
    void mergeTools(const std::vector<ContainerToolSpec> &tools);

    std::vector<ContainerToolSpec> tools_;
    std::map<std::string, bool> neverRemind_;
    std::filesystem::path systemConfigPath_;
    std::filesystem::path userConfigPath_;
};

} // namespace linglong::ctk::detect
