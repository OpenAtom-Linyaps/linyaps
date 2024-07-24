// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>

#include <pwd.h>
#include <unistd.h>

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
        std::cerr << "unknown error occurred during parsing json." << std::endl;
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return -1;
    }

    nlohmann::json annotations;
    std::string appID;
    try {
        annotations = content.at("annotations");
        appID = annotations.at("org.deepin.linglong.appID");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    }

    auto &mounts = content["mounts"];
    auto &env = content["process"]["env"];

    auto *homeEnv = ::getenv("HOME");
    auto *userNameEnv = ::getenv("USER");
    if (homeEnv == nullptr || userNameEnv == nullptr) {
        std::cerr << "Couldn't get HOME or USER from env." << std::endl;
        return -1;
    }

    auto hostHomeDir = std::filesystem::path(homeEnv);
    auto cognitiveHomeDir = std::filesystem::path{ "/home" } / userNameEnv;
    if (!std::filesystem::exists(hostHomeDir)) {
        std::cerr << "Home " << hostHomeDir << "doesn't exists." << std::endl;
        return -1;
    }

    mounts.push_back({
      { "destination", "/home" },
      { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
      { "source", "tmpfs" },
      { "type", "tmpfs" },
    });

    auto envExist = [&env](const std::string &key) {
        auto prefix = key + "=";
        auto it = std::find_if(env.cbegin(), env.cend(), [&prefix](const std::string &item) {
            return (item.rfind(prefix, 0) == 0);
        });
        return it != env.cend();
    };

    auto mountDir =
      [&mounts](const std::string &hostDir, const std::string &destDir, std::error_code &ec) {
          std::string realHostDir = hostDir;
          if (std::filesystem::is_symlink(hostDir)) {
              realHostDir = std::filesystem::read_symlink(hostDir);
          }

          std::filesystem::create_directories(realHostDir, ec);
          if (ec) {
              return;
          }

          std::filesystem::create_directories(destDir, ec);
          if (ec) {
              return;
          }

          mounts.push_back({
            { "destination", destDir },
            { "options", nlohmann::json::array({ "rbind" }) },
            { "source", realHostDir },
            { "type", "bind" },
          });

          ec.clear();
      };

    std::error_code ec;
    mountDir(hostHomeDir, cognitiveHomeDir, ec);
    if (ec) {
        std::cerr << "Mount home failed:" << ec.message() << std::endl;
        return -1;
    }
    if (envExist("HOME")) {
        std::cerr << "HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("HOME=" + cognitiveHomeDir.string());

    auto hostAppDataDir = std::filesystem::path(hostHomeDir / ".linglong" / appID);
    std::filesystem::create_directories(hostAppDataDir, ec);
    if (ec) {
        std::cerr << "Check appDataDir failed:" << ec.message() << std::endl;
        return -1;
    }

    // process XDG_* environment variables.

    // Data files should access by other application.
    auto *ptr = ::getenv("XDG_DATA_HOME");
    auto XDGDataHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGDataHome.empty()) {
        XDGDataHome = hostHomeDir / ".local/share";
    }

    auto cognitiveXDGDataHome = (cognitiveHomeDir / ".local/share").string();
    mountDir(XDGDataHome, cognitiveXDGDataHome, ec);
    if (ec) {
        std::cerr << "Failed to passthrough " << XDGDataHome << ec.message() << std::endl;
        return -1;
    }
    if (envExist("XDG_DATA_HOME")) {
        std::cerr << "XDG_DATA_HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("XDG_DATA_HOME=" + cognitiveXDGDataHome);

    auto hostAppConfigHome = hostAppDataDir / "config";
    auto cognitiveAppConfigHome = cognitiveHomeDir / ".config";
    mountDir(hostAppConfigHome, cognitiveAppConfigHome, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostAppConfigHome << " to " << cognitiveAppConfigHome
                  << ec.message() << std::endl;
        return -1;
    }
    if (envExist("XDG_CONFIG_HOME")) {
        std::cerr << "XDG_CONFIG_HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("XDG_CONFIG_HOME=" + cognitiveAppConfigHome.string());

    auto hostAppCacheHome = hostAppDataDir / "cache";
    auto cognitiveAppCacheHome = cognitiveHomeDir / ".cache";
    mountDir(hostAppCacheHome, cognitiveAppCacheHome, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostAppCacheHome << " to " << cognitiveAppCacheHome
                  << ec.message() << std::endl;
        return -1;
    }
    if (envExist("XDG_CACHE_HOME")) {
        std::cerr << "XDG_CACHE_HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("XDG_CACHE_HOME=" + cognitiveAppCacheHome.string());

    auto hostAppStateHome = hostAppDataDir / "state";
    auto cognitiveAppStateHome = cognitiveHomeDir / ".local" / "state";
    mountDir(hostAppStateHome, cognitiveAppStateHome, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostAppStateHome << " to " << cognitiveAppStateHome
                  << ec.message() << std::endl;
        return -1;
    }
    if (envExist("XDG_STATE_HOME")) {
        std::cerr << "XDG_STATE_HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("XDG_STATE_HOME=" + cognitiveAppStateHome.string());

    // systemd user path
    auto hostSystemdUserDir = hostAppConfigHome / "systemd/user";
    auto cognitiveSystemdUserDir = cognitiveAppConfigHome / "systemd/user";
    mountDir(hostSystemdUserDir, cognitiveSystemdUserDir, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostSystemdUserDir << " to " << cognitiveSystemdUserDir
                  << ec.message() << std::endl;
        return -1;
    }

    // FIXME: Many applications get configurations from dconf, so we expose dconf to all
    // applications for now. If there is a better solution to fix this issue, please change the
    // following codes
    auto XDGUserConfig = hostHomeDir / ".config";
    if (auto *ptr = ::getenv("XDG_CONFIG_HOME"); ptr != nullptr) {
        XDGUserConfig = ptr;
    }
    auto hostUserDconfPath = XDGUserConfig / "dconf";
    auto cognitiveAppDconfPath = cognitiveAppConfigHome / "dconf";
    mountDir(hostUserDconfPath, cognitiveAppDconfPath, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostUserDconfPath << " to " << cognitiveAppDconfPath
                  << ec.message() << std::endl;
        return -1;
    }

    // for dde application theme
    ptr = ::getenv("XDG_CACHE_HOME");
    auto hostXDGCacheHome = ptr == nullptr ? "" : std::string{ ptr };
    if (hostXDGCacheHome.empty()) {
        hostXDGCacheHome = hostHomeDir / ".cache";
    }

    auto hostDDEApiPath = std::filesystem::path{ hostXDGCacheHome } / "deepin/dde-api";
    auto cognitiveDDEApiPath = cognitiveAppCacheHome / "deepin/dde-api";
    mountDir(hostDDEApiPath, cognitiveDDEApiPath, ec);
    if (ec) {
        std::cerr << "Failed to mount " << hostDDEApiPath << " to " << cognitiveDDEApiPath
                  << ec.message() << std::endl;
        return -1;
    }

    // for xdg-user-dirs
    if (auto userDirs = hostHomeDir / ".config/user-dirs.dirs"; std::filesystem::exists(userDirs)) {
        mounts.push_back({
          { "destination", userDirs },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", userDirs },
          { "type", "bind" },
        });
    }

    if (auto userLocale = hostHomeDir / ".config/user-dirs.locale";
        std::filesystem::exists(userLocale)) {
        mounts.push_back({
          { "destination", userLocale },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", userLocale },
          { "type", "bind" },
        });
    }

    // NOTE:
    // Running ~/.bashrc from user home is meaningless in linglong container,
    // and might cause some issues, so we mask it with the default one.
    // https://github.com/linuxdeepin/linglong/issues/459
    constexpr auto defaultBashrc = "/etc/skel/.bashrc";
    if (std::filesystem::exists(defaultBashrc)) {
        mounts.push_back({
          { "destination", hostHomeDir / ".bashrc" },
          { "options", nlohmann::json::array({ "ro", "rbind" }) },
          { "source", defaultBashrc },
          { "type", "bind" },
        });
    } else {
        std::cerr << "failed to mask bashrc" << std::endl;
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
