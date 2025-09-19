// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/bash_quote.h"

#include <sstream>
#include <string>
#include <vector>

namespace linglong::utils {

class BashCommandHelper
{
public:
    /**
     * @brief 生成用于启动一个纯净 Bash shell 的基础命令行参数列表。
     *
     * 该函数构建一个字符串向量，代表启动一个 Bash 进程所需的基础命令和参数。
     * 这个进程不继承父进程的环境变量，并且不自动加载任何用户的配置文件。
     * 它为后续需要在一个可预测、无用户自定义配置影响的环境中执行命令的场景提供基础。
     *
     * @return 包含 'bash --noprofile --norc -c' 的字符串向量。
     */
    inline static std::vector<std::string> generateBashCommandBase()
    {
        return {
            "/bin/bash",   // 在干净环境中执行的 Shell。
            "--noprofile", // 'bash' 的参数，防止它加载 .profile、.bash_profile 等登录脚本。
            "--norc",      // 'bash' 的另一个参数，防止它加载 .bashrc 等交互式启动脚本。
            "-c"           // 'bash' 的参数，用于执行一个单一的命令字符串。
        };
    }

    /**
     * @brief 生成用于启动一个完整的、默认 Bash shell 的命令参数列表。
     *
     * 该函数调用 generateBashCommandBase 来获得基础命令参数，
     * 然后在其后添加一个命令字符串，用于：
     * 1. 手动加载系统级的 /etc/profile 文件，以确保 PATH 等关键环境变量被正确设置。
     * 2. 最终启动一个新的、交互式的 Bash shell，并且该 shell 不会加载用户的 .bashrc。
     *
     * 这个命令列表可用于启动一个干净但功能完备的 Bash 会话，
     * 例如在容器或沙盒环境中。
     *
     * @return 包含完整命令和参数的字符串向量，其概念上等同于
     * 'bash --noprofile --norc -c "source /etc/profile; bash --norc"'。
     */
    inline static std::vector<std::string> generateDefaultBashCommand()
    {
        auto cmd = generateBashCommandBase();
        cmd.push_back("source /etc/profile; bash --norc"); // 手动加载系统级的 profile
                                                           // 脚本。这确保了在干净的环境中， 像 PATH
                                                           // 这样关键的系统变量被正确设置。
        return cmd;
    }

    /**
     * @brief 生成在干净的 Bash 环境中执行命令的命令行参数。
     *
     * 该函数构建一个字符串向量，代表子进程的命令及其参数。
     * 它旨在在 Linglong 容器内启动一个可执行程序，同时确保 Shell 环境是可预测的，
     * 并且不受用户特定配置的影响。
     *
     * @param entrypoint 一个包含将由 Bash Shell 执行的命令的字符串。
     * 这个命令通常是一个 Shell 脚本或要运行的程序。
     * @return 包含程序路径及其参数的字符串向量。
     * 概念上，生成的命令序列是：
     * `/run/linglong/container-init /bin/bash --noprofile --norc -c "<entrypoint>"`
     */
    inline static std::vector<std::string> generateExecCommand(const std::string &entrypoint)
    {
        // TODO: maybe we could use a symlink '/usr/bin/ll-init' points to
        // '/run/linglong/container-init' will be better
        auto cmd = generateBashCommandBase();
        cmd.insert(cmd.begin(), "/run/linglong/container-init");
        cmd.push_back(entrypoint);
        return cmd;
    }

    /**
     * @brief 从参数列表生成一个完整的 Bash 脚本字符串。
     *
     * 该函数接收一个字符串向量作为命令行参数，并构建一个可作为入口点的单一 Bash 脚本。
     * 生成的脚本首先加载 /etc/profile 以建立基准系统环境，然后执行带参数的命令。
     *
     * @param args 一个代表命令及其参数的字符串向量。
     * @return 包含完整 Bash 脚本的单一字符串。
     */
    inline static std::string generateEntrypointScript(const std::vector<std::string> &args)
    {
        std::stringstream script;
        script << "#!/bin/bash\n";
        script << "source /etc/profile\n";
        script << "exec ";
        for (const auto &arg : args) {
            script << quoteBashArg(arg) << " ";
        }
        return script.str();
    }
};

} // namespace linglong::utils
