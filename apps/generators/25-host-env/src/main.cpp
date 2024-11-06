// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>

extern char **environ;

const std::vector<std::string> envList = {
    "DISPLAY",
    "LANG",
    "LANGUAGE",
    "XDG_SESSION_DESKTOP",
    "D_DISABLE_RT_SCREEN_SCALE",
    "XMODIFIERS",
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
    "GNOME_DESKTOP_SESSION_ID" // gnome 桌面标识，有些应用会读取此变量以使用gsettings配置, 如chrome
};

int main()
{
    nlohmann::json content;
    std::string ociVersion;
    try {
        content = nlohmann::json::parse(std::cin);
        ociVersion = content.at("ociVersion");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown error occurred during parsing json." << std::endl;
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return -1;
    }

    auto &env = content["process"]["env"];

    // get the environment variables of current process
    for (const auto &filter : envList) {
        for (int i = 0; environ[i] != nullptr; ++i) {
            if (std::strncmp(environ[i], filter.c_str(), filter.length()) == 0
                && environ[i][filter.length()] == '=') {
                // check if the value part is not empty
                if (environ[i][filter.length() + 1] != '\0') {
                    env.emplace_back(environ[i]);
                }
            }
        }
    }

    auto annotations = content.at("annotations");
    env.push_back("LINGLONG_APPID="
                  + annotations.at("org.deepin.linglong.appID").get<std::string>());

    std::cout << content.dump() << std::endl;
    return 0;
}
