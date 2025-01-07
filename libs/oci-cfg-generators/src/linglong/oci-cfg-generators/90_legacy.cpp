// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "90_legacy.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace linglong::generator {

bool Legacy::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (config.ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return false;
    }

    if (!config.annotations) {
        std::cerr << "no annotations." << std::endl;
        return false;
    }

    auto onlyApp = config.annotations->find("org.deepin.linglong.onlyApp");
    if (onlyApp != config.annotations->end() && onlyApp->second == "true") {
        return true;
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
    auto mounts = config.mounts.value_or(std::vector<ocppi::runtime::config::types::Mount>{});

    // FIXME: time zone in the container does not change when the host time zone changes，need to be
    // repaired later.
    std::multimap<std::string_view, std::string_view> roMountMap{
        { "/etc/resolvconf", "/run/host/etc/resolvconf" },
        { "/etc/machine-id", "/run/host/etc/machine-id" },
        { "/etc/machine-id", "/etc/machine-id" },
        { "/etc/ssl/certs", "/run/host/etc/ssl/certs" },
        { "/etc/ssl/certs", "/etc/ssl/certs" },
        { "/var/cache/fontconfig", "/run/host/appearance/fonts-cache" },
        // FIXME: app can not display normally due to missing cjk font cache file,so we need bind
        // /var/cache/fontconfig to container. this is just a temporary solution,need to be removed
        // when font cache solution implemented
        { "/var/cache/fontconfig", "/var/cache/fontconfig" },
        { "/usr/share/fonts", "/usr/share/fonts" },
        { "/usr/lib/locale/", "/usr/lib/locale/" },
        { "/usr/share/themes", "/usr/share/themes" },
        { "/usr/share/icons", "/usr/share/icons" },
        { "/usr/share/zoneinfo", "/usr/share/zoneinfo" },
        { "/etc/resolvconf", "/etc/resolvconf" },
    };

    for (const auto [source, destination] : roMountMap) {
        if (!std::filesystem::exists(source)) {
            std::cerr << source << " not exists on host." << std::endl;
            continue;
        }

        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = std::string{ destination },
          .options = string_list{ "ro", "rbind" },
          .source = std::string{ source },
          .type = "bind",
        });
    };

    {
        // FIXME: com.360.browser-stable
        // 需要一个所有用户都有可读可写权限的目录(/apps-data/private/com.360.browser-stable)
        if (::getenv("LINGLONG_SKIP_HOME_GENERATE") == nullptr
            && "com.360.browser-stable" == appID->second) {
            auto *home = ::getenv("HOME");
            if (home == nullptr) {
                std::cerr << "Couldn't get HOME." << std::endl;
                return -1;
            }

            auto homeDir = std::filesystem::path(home);
            if (!std::filesystem::exists(homeDir)) {
                std::cerr << "Home " << homeDir << "doesn't exists." << std::endl;
                return -1;
            }

            std::error_code ec;
            std::string app360DataSourcePath =
              homeDir / ".linglong" / appID->second / "share" / "appdata";

            auto appDataDir = std::filesystem::path(app360DataSourcePath);
            std::filesystem::create_directories(appDataDir, ec);
            if (ec) {
                std::cerr << "Check appDataDir failed:" << ec.message() << std::endl;
                return -1;
            }

            std::string app360DataPath = "/apps-data";
            std::string app360DataDestPath = app360DataPath + "/private/com.360.browser-stable";

            mounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = std::move(app360DataPath),
              .options = string_list{ " nodev ", " nosuid ", " mode = 777 " },
              .source = "tmpfs",
              .type = "tmpfs",
            });

            mounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = std::move(app360DataDestPath),
              .options = string_list{ "rw", "rbind" },
              .source = std::move(app360DataSourcePath),
              .type = "bind",
            });
        }
    }

    // randomize mount points to avoid path dependency.
    auto now = std::chrono::system_clock::now();
    auto timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto shareDir = std::filesystem::path("/run/linglong/usr/share_" + std::to_string(timestamp));
    // add mount points to XDG_DATA_DIRS
    // 查找是否存在XDG_DATA_DIRS开头的环境变量，如果存在追加到尾部，不存在则添加
    auto it = std::find_if(env.begin(), env.end(), [](const std::string &var) {
        return var.find("XDG_DATA_DIRS=") == 0;
    });
    if (it != env.end()) {
        // 如果存在，追加到尾部
        *it += ":" + shareDir.string();
    } else {
        // 如果不存在，添加到末尾
        env.push_back("XDG_DATA_DIRS=" + shareDir.string());
    }

    std::error_code ec;
    // mount for dtk
    if (std::filesystem::exists("/usr/share/deepin/distribution.info", ec)) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = shareDir / "deepin/distribution.info",
          .options = string_list{ "nodev", "nosuid", "mode=0644" },
          .source = "/usr/share/deepin/distribution.info",
          .type = "bind",
        });
    }

    process.env = std::move(env);
    config.process = std::move(process);
    config.mounts = std::move(mounts);

    return true;
}

} // namespace linglong::generator
