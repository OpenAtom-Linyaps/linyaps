// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "25_host_env.h"

#include <iostream>

#include <unistd.h>

namespace linglong::generator {

bool HostEnv::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (config.ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return false;
    }

    if (!config.annotations) {
        std::cerr << "no annotations." << std::endl;
        return false;
    }

    auto appID = config.annotations->find("org.deepin.linglong.appID");
    if (appID == config.annotations->end()) {
        std::cerr << "appID not found." << std::endl;
        return false;
    }

    if (appID->second.empty()) {
        std::cerr << "appID is empty." << std::endl;
        return false;
    }

    auto process = config.process.value_or(ocppi::runtime::config::types::Process{});
    auto env = process.env.value_or(std::vector<std::string>{});

    env.push_back("LINGLONG_APPID=" + appID->second);

    const std::vector<std::string> envList = {
        "DISPLAY",
        "LANG",
        "LANGUAGE",
        "XDG_SESSION_DESKTOP",
        "D_DISABLE_RT_SCREEN_SCALE",
        "XMODIFIERS",
        "XCURSOR_SIZE", // 鼠标尺寸
        "DESKTOP_SESSION",
        "DEEPIN_WINE_SCALE",
        "XDG_CURRENT_DESKTOP",
        "XIM",
        "XDG_SESSION_TYPE",
        "XDG_RUNTIME_DIR",
        "CLUTTER_IM_MODULE",
        "QT4_IM_MODULE",
        "GTK_IM_MODULE",
        "auto_proxy",      // 网络系统代理自动代理
        "http_proxy",      // 网络系统代理手动http代理
        "https_proxy",     // 网络系统代理手动https代理
        "ftp_proxy",       // 网络系统代理手动ftp代理
        "SOCKS_SERVER",    // 网络系统代理手动socks代理
        "no_proxy",        // 网络系统代理手动配置代理
        "USER",            // wine应用会读取此环境变量
        "QT_IM_MODULE",    // 输入法
        "LINGLONG_ROOT",   // 玲珑安装位置
        "WAYLAND_DISPLAY", // 导入wayland相关环境变量
        "QT_QPA_PLATFORM",
        "QT_WAYLAND_SHELL_INTEGRATION",
        "GDMSESSION",
        "QT_WAYLAND_FORCE_DPI",
        "GIO_LAUNCHED_DESKTOP_FILE", // 系统监视器
        "GNOME_DESKTOP_SESSION_ID",  // gnome 桌面标识，有些应用会读取此变量以使用gsettings配置,
                                     // 如chrome
        "TERM"
    };

    auto onlyApp = config.annotations->find("org.deepin.linglong.onlyApp");
    if (onlyApp != config.annotations->end() && onlyApp->second == "true") {
        // 如果是 onlyApp 模式，直接追加所有环境变量并返回
        for (char **envp = environ; *envp != nullptr; envp++) {
            env.emplace_back(*envp);
        }
    } else {
        // get the environment variables of current process
        for (const auto &filter : envList) {
            auto *host = ::getenv(filter.data());
            if (host != nullptr) {
                env.emplace_back(std::string{ filter } + "=" + host);
            }
        }
    }

    process.env = std::move(env);
    config.process = std::move(process);

    return true;
}

} // namespace linglong::generator
