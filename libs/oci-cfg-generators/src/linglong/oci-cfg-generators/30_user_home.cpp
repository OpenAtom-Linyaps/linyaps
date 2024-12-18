// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "30_user_home.h"

#include "linglong/api/types/v1/Generators.hpp"

#include <linux/limits.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include <pwd.h>
#include <unistd.h>

// TODO: resolve xdg-user-dir config
std::string resolveXDGDir(const std::string &dirType)
{
    return dirType;
}

namespace linglong::generator {

bool UserHome::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (::getenv("LINGLONG_SKIP_HOME_GENERATE") != nullptr) {
        return true;
    }

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

    auto mounts = config.mounts.value_or(std::vector<ocppi::runtime::config::types::Mount>{});
    auto process = config.process.value_or(ocppi::runtime::config::types::Process{});
    auto env = process.env.value_or(std::vector<std::string>{});

    auto *homeEnv = ::getenv("HOME");
    auto *userNameEnv = ::getenv("USER");
    if (homeEnv == nullptr || userNameEnv == nullptr) {
        std::cerr << "Couldn't get HOME or USER from env." << std::endl;
        return false;
    }

    auto hostHomeDir = std::filesystem::path(homeEnv);
    auto cognitiveHomeDir = std::filesystem::path{ "/home" } / userNameEnv;
    if (!std::filesystem::exists(hostHomeDir)) {
        std::cerr << "Home " << hostHomeDir << "doesn't exists." << std::endl;
        return false;
    }

    mounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/home",
      .options = string_list{ "nodev", "nosuid", "mode=700" },
      .source = "tmpfs",
      .type = "tmpfs",
    });

    auto envExist = [&env](const std::string &key) {
        auto prefix = key + "=";
        auto it = std::find_if(env.cbegin(), env.cend(), [&prefix](const std::string &item) {
            return (item.rfind(prefix, 0) == 0);
        });
        return it != env.cend();
    };

    auto mountDir = [&mounts](const std::string &hostDir, const std::string &containerDir) {
        std::error_code ec;
        if (!std::filesystem::exists(hostDir, ec)) {
            if (ec) {
                std::cerr << "failed to get state of directories " << hostDir << ":" << ec.message()
                          << std::endl;
                return false;
            }

            if (!std::filesystem::create_directories(hostDir, ec) && ec) {
                std::cerr << "failed to create directories " << hostDir << ":" << ec.message()
                          << std::endl;
                return false;
            }
        }

        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = containerDir,
          .options = string_list{ "rbind" },
          .source = hostDir,
          .type = "bind",
        });

        return true;
    };

    if (!mountDir(hostHomeDir, cognitiveHomeDir)) {
        return false;
    }
    if (envExist("HOME")) {
        std::cerr << "HOME already exist." << std::endl;
        return false;
    }
    env.emplace_back("HOME=" + cognitiveHomeDir.string());

    // process XDG_* environment variables.
    std::error_code ec;
    auto privateAppDir = hostHomeDir / ".linglong" / appID->second;

    if (!std::filesystem::create_directories(privateAppDir, ec) && ec) {
        std::cerr << "failed to create " << privateAppDir << ": " << ec.message() << std::endl;
        return false;
    }

    // XDG_DATA_HOME
    auto *ptr = ::getenv("XDG_DATA_HOME");
    std::filesystem::path XDGDataHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGDataHome.empty()) {
        XDGDataHome = hostHomeDir / ".local" / "share";
    }

    std::filesystem::path cognitiveDataHome = cognitiveHomeDir / ".local" / "share";
    if (!mountDir(XDGDataHome, cognitiveDataHome)) {
        return false;
    }

    if (envExist("XDG_DATA_HOME")) {
        std::cerr << "XDG_DATA_HOME already exist." << std::endl;
        return false;
    }
    env.emplace_back("XDG_DATA_HOME=" + cognitiveDataHome.string());

    // XDG_CONFIG_HOME
    ptr = ::getenv("XDG_CONFIG_HOME");
    std::filesystem::path XDGConfigHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGConfigHome.empty()) {
        XDGConfigHome = hostHomeDir / ".config";
    }
    if (auto privateConfigDir = privateAppDir / "config";
        std::filesystem::exists(privateConfigDir, ec)) {
        XDGConfigHome = privateConfigDir;
    }
    ec.clear();

    auto cognitiveConfigHome = cognitiveHomeDir / ".config";
    if (!mountDir(XDGConfigHome, cognitiveConfigHome)) {
        return false;
    }

    if (envExist("XDG_CONFIG_HOME")) {
        std::cerr << "XDG_CONFIG_HOME already exist." << std::endl;
        return false;
    }
    env.emplace_back("XDG_CONFIG_HOME=" + cognitiveConfigHome.string());

    // XDG_CACHE_HOME
    ptr = ::getenv("XDG_CACHE_HOME");
    std::filesystem::path XDGCacheHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGCacheHome.empty()) {
        XDGCacheHome = hostHomeDir / ".cache";
    }
    if (auto privateCacheDir = privateAppDir / "cache";
        std::filesystem::exists(privateCacheDir, ec)) {
        XDGCacheHome = privateCacheDir;
    }
    ec.clear();

    auto cognitiveCacheHome = cognitiveHomeDir / ".cache";
    if (!mountDir(XDGCacheHome, cognitiveCacheHome)) {
        return false;
    }

    if (envExist("XDG_CACHE_HOME")) {
        std::cerr << "XDG_CACHE_HOME already exist." << std::endl;
        return false;
    }
    env.emplace_back("XDG_CACHE_HOME=" + cognitiveCacheHome.string());

    // XDG_STATE_HOME
    ptr = ::getenv("XDG_STATE_HOME");
    std::filesystem::path XDGStateHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGStateHome.empty()) {
        XDGStateHome = hostHomeDir / ".local" / "state";
    }
    if (auto privateStateDir = privateAppDir / "config";
        std::filesystem::exists(privateStateDir, ec)) {
        XDGStateHome = privateStateDir;
    }
    ec.clear();

    auto cognitiveStateHome = cognitiveHomeDir / ".local" / "state";
    if (!mountDir(XDGStateHome, cognitiveStateHome)) {
        return false;
    }

    if (envExist("XDG_STATE_HOME")) {
        std::cerr << "XDG_STATE_HOME already exist." << std::endl;
        return false;
    }
    env.emplace_back("XDG_STATE_HOME=" + cognitiveStateHome.string());

    // systemd user path
    auto hostSystemdUserDir = XDGConfigHome / "systemd" / "user";
    if (std::filesystem::exists(hostSystemdUserDir, ec)) {
        auto cognitiveSystemdUserDir = cognitiveConfigHome / "systemd" / "user";
        if (!mountDir(hostSystemdUserDir, cognitiveSystemdUserDir)) {
            return false;
        }
    }

    // FIXME: Many applications get configurations from dconf, so we expose dconf to all
    // applications for now. If there is a better solution to fix this issue, please change the
    // following codes
    auto hostUserDconfPath = XDGConfigHome / "dconf";
    if (std::filesystem::exists(hostUserDconfPath, ec)) {
        auto cognitiveAppDconfPath = cognitiveConfigHome / "dconf";
        if (!mountDir(hostUserDconfPath, cognitiveAppDconfPath)) {
            return false;
        }
    }

    // for dde application theme
    auto hostDDEApiPath = XDGCacheHome / "deepin" / "dde-api";
    if (std::filesystem::exists(hostDDEApiPath, ec)) {
        auto cognitiveDDEApiPath = cognitiveCacheHome / "deepin" / "dde-api";
        if (!mountDir(hostDDEApiPath, cognitiveDDEApiPath)) {
            return false;
        }
    }

    // for xdg-user-dirs
    auto XDGUserDirs = XDGConfigHome / "user-dirs.dirs";
    if (std::filesystem::exists(XDGUserDirs, ec)) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = cognitiveConfigHome / "user-dirs.dirs",
          .options = string_list{ "rbind" },
          .source = XDGUserDirs,
          .type = "bind",
        });
    }

    auto XDGUserLocale = XDGConfigHome / "user-dirs.locale";
    if (std::filesystem::exists(XDGUserLocale, ec)) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = cognitiveConfigHome / "user-dirs" / ".locale",
          .options = string_list{ "rbind" },
          .source = XDGUserLocale,
          .type = "bind",
        });
    }

    // NOTE:
    // Running ~/.bashrc from user home is meaningless in linglong container,
    // and might cause some issues, so we mask it with the default one.
    // https://github.com/linuxdeepin/linglong/issues/459
    constexpr auto defaultBashrc = "/etc/skel/.bashrc";
    if (std::filesystem::exists(defaultBashrc)) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = hostHomeDir / ".bashrc",
          .options = string_list{ "ro", "rbind" },
          .source = defaultBashrc,
          .type = "bind",
        });
    } else {
        std::cerr << "failed to mask bashrc" << std::endl;
    }

    // hide self data
    auto linglongMaskDataDir = hostHomeDir / ".linglong" / "data";
    if (!mountDir(linglongMaskDataDir, cognitiveHomeDir / ".linglong")) {
        return false;
    }

    auto permissions = linglong::api::types::v1::ApplicationConfigurationPermissions{};
    auto configFile = privateAppDir / "permissions.json";
    if (!std::filesystem::exists(configFile, ec)) {
        if (ec) {
            std::cerr << "failed to get status of " << configFile.c_str() << ": " << ec.message()
                      << std::endl;
            return false;
        }

        // no permission config, do nothing
        process.env = std::move(env);
        config.process = std::move(process);
        config.mounts = std::move(mounts);
        return true;
    }

    auto input = std::ifstream(configFile);
    if (!input.is_open()) {
        std::cerr << "couldn't open config file " << configFile.c_str() << std::endl;
        return false;
    }

    try {
        auto content = nlohmann::json::parse(input);
        permissions = content.get<linglong::api::types::v1::ApplicationConfigurationPermissions>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "deserialize error:" << e.what() << std::endl;
        return false;
    } catch (std::exception &e) {
        std::cerr << "unknown exception:" << e.what() << std::endl;
        return false;
    }

    auto directories = permissions.xdgDirectories.value_or(
      std::vector<linglong::api::types::v1::XdgDirectoryPermission>{});

    // FIXME: we should resolve real home through env GNUPGHOME
    // FIXME: we should resolve user dirs through ${XDG_CONFIG_HOME}/user-dirs.dirs
    std::vector<std::string> blacklist = { ".gnupg", ".ssh" };
    for (const auto &[allowed, dirType] : directories) {
        if (!allowed) {
            blacklist.push_back(resolveXDGDir(dirType));
        }
    }

    auto it =
      std::remove_if(blacklist.begin(), blacklist.end(), [&hostHomeDir](const std::string &dir) {
          std::error_code ec;
          auto ret = !std::filesystem::exists(hostHomeDir / dir, ec);
          if (ec) {
              std::cerr << "failed to get state of " << hostHomeDir / dir << ": " << ec.message()
                        << std::endl;
          }
          return ret;
      });
    blacklist.erase(it);

    for (const auto &relative : blacklist) {
        if (!mountDir(privateAppDir / relative, cognitiveHomeDir / relative)) {
            return false;
        }
    }

    process.env = std::move(env);
    config.process = std::move(process);
    config.mounts = std::move(mounts);
    return true;
}

} // namespace linglong::generator
