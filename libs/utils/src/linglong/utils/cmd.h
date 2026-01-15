/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace linglong::utils {

// Executes a command from the standard system PATH
// blocking until the child process exits, and the stdout of the child process is returned
class Cmd
{
public:
    explicit Cmd(std::string command) noexcept;
    ~Cmd();

    virtual bool exists() noexcept;
    virtual utils::error::Result<std::string>
    exec(const std::vector<std::string> &args = {}) noexcept;
    virtual Cmd &setEnv(const std::string &name, const std::string &value) noexcept;
    virtual Cmd &toStdin(std::string content) noexcept;

private:
    std::filesystem::path getCommandPath();

    std::string m_command;
    std::map<std::string, std::string> m_envs;
    std::string m_stdinContent;
};

} // namespace linglong::utils
