// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>

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
    std::string baseDir;
    std::string appID;
    try {
        annotations = content.at("annotations");
        baseDir = annotations.at("org.deepin.linglong.baseDir");
        appID = annotations.at("org.deepin.linglong.appID");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    }

    auto &mounts = content["mounts"];

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

    mounts.push_back({
      { "destination", "/home" },
      { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
      { "source", "tmpfs" },
      { "type", "tmpfs" },
    });

    auto PassthroughDir = [&mounts](const std::string &absolutePath, std::error_code &ec) {
        std::filesystem::create_directories(absolutePath, ec);
        if (ec) {
            return;
        }

        mounts.push_back({
          { "destination", absolutePath },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", absolutePath },
          { "type", "bind" },
        });

        ec.clear();
    };

    std::error_code ec;
    PassthroughDir(homeDir.c_str(), ec);
    if (ec) {
        std::cerr << "Mount home failed:" << ec.message() << std::endl;
        return -1;
    }

    PassthroughDir(homeDir / ".deepinwine", ec);
    if (ec) {
        std::cerr << "Mount .deepinwine failed:" << ec.message() << std::endl;
        return -1;
    }

    auto appDataDir = std::filesystem::path(homeDir / ".linglong" / appID);
    std::filesystem::create_directories(appDataDir, ec);
    if (ec) {
        std::cerr << "Check appDataDir failed:" << ec.message() << std::endl;
        return -1;
    }

    auto shadowDir =
      [&mounts](const std::string &hostDir, const std::string &destDir, std::error_code &ec) {
          std::filesystem::create_directories(hostDir, ec);
          if (ec) {
              return;
          }

          std::filesystem::create_directories(destDir, ec);
          if (ec) {
              return;
          }

          mounts.push_back({
            { "destination", hostDir },
            { "options", nlohmann::json::array({ "rbind" }) },
            { "source", destDir },
            { "type", "bind" },
          });

          ec.clear();
      };

    // process XDG_* environment variables.

    // Data files should access by other application.
    auto *ptr = ::getenv("XDG_DATA_HOME");
    auto XDGDataHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGDataHome.empty()) {
        XDGDataHome = homeDir / ".local/share";
    }
    PassthroughDir(XDGDataHome, ec);
    if (ec) {
        std::cerr << "Failed to passthrough " << XDGDataHome << ec.message() << std::endl;
        return -1;
    }

    ptr = ::getenv("XDG_CONFIG_HOME");
    auto XDGConfigHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGConfigHome.empty()) {
        XDGConfigHome = homeDir / ".config";
    }

    auto appConfigDir = appDataDir / "config";
    shadowDir(XDGConfigHome, appConfigDir, ec);
    if (ec) {
        std::cerr << "Failed to shadow " << XDGConfigHome << ec.message() << std::endl;
        return -1;
    }

    ptr = ::getenv("XDG_CACHE_HOME");
    auto XDGCacheHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGCacheHome.empty()) {
        XDGCacheHome = homeDir / ".cache";
    }
    shadowDir(XDGCacheHome, appDataDir / "cache", ec);
    if (ec) {
        std::cerr << "Failed to shadow " << XDGCacheHome << ec.message() << std::endl;
        return -1;
    }

    ptr = ::getenv("XDG_STATE_HOME");
    auto XDGStateHome = ptr == nullptr ? "" : std::string{ ptr };
    ;
    if (XDGStateHome.empty()) {
        XDGStateHome = homeDir / ".local/state";
    }
    shadowDir(XDGStateHome, appDataDir / "state", ec);
    if (ec) {
        std::cerr << "Failed to shadow " << XDGStateHome << ec.message() << std::endl;
        return -1;
    }

    // systemd user path
    auto systemdUserDir = XDGConfigHome + "/systemd/user";
    shadowDir(systemdUserDir, appDataDir / "config/systemd/user", ec);
    if (ec) {
        std::cerr << "Failed to shadow " << systemdUserDir << ec.message() << std::endl;
        return -1;
    }

    auto dconfPath = XDGConfigHome + "/dconf";
    shadowDir(dconfPath, appDataDir / "config/dconf", ec);
    if (ec) {
        std::cerr << "Failed to shadow " << dconfPath << ec.message() << std::endl;
        return -1;
    }

    // for dde application theme
    auto ddeApiPath = XDGCacheHome + "/deepin/dde-api";
    PassthroughDir(ddeApiPath, ec);
    if (ec) {
        std::cerr << "Failed to passthrough " << ddeApiPath << ec.message() << std::endl;
        return -1;
    }
    shadowDir(ddeApiPath, appConfigDir / "deepin/dde-api", ec);
    if (ec) {
        std::cerr << "Failed to shadow " << ddeApiPath << ec.message() << std::endl;
        return -1;
    }

    // for xdg-user-dirs
    if (auto userDirs = homeDir / ".config/user-dirs.dirs"; std::filesystem::exists(userDirs)) {
        mounts.push_back({
          { "destination", userDirs },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", userDirs },
          { "type", "bind" },
        });
    }

    if (auto userLocale = homeDir / ".config/user-dirs.locale";
        std::filesystem::exists(userLocale)) {
        mounts.push_back({
          { "destination", userLocale },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", userLocale },
          { "type", "bind" },
        });
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
