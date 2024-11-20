// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/ApplicationAccessPrivileges.hpp"
#include "linglong/api/types/v1/Generators.hpp"
#include "nlohmann/json.hpp"

#include <linux/limits.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include <pwd.h>
#include <unistd.h>

int main() // NOLINT
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

        mounts.push_back({
          { "destination", containerDir },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", hostDir },
          { "type", "bind" },
        });

        return true;
    };

    if (!mountDir(hostHomeDir, cognitiveHomeDir)) {
        return -1;
    }
    if (envExist("HOME")) {
        std::cerr << "HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("HOME=" + cognitiveHomeDir.string());

    // process XDG_* environment variables.
    std::error_code ec;
    auto privateAppDir = hostHomeDir / ".linglong" / appID;

    if (!std::filesystem::create_directories(privateAppDir, ec) && ec) {
        std::cerr << "failed to create " << privateAppDir << ": " << ec.message() << std::endl;
        return -1;
    }

    // XDG_DATA_HOME
    auto *ptr = ::getenv("XDG_DATA_HOME");
    std::filesystem::path XDGDataHome = ptr == nullptr ? "" : std::string{ ptr };
    if (XDGDataHome.empty()) {
        XDGDataHome = hostHomeDir / ".local" / "share";
    }

    std::filesystem::path cognitiveDataHome = cognitiveHomeDir / ".local" / "share";
    if (!mountDir(XDGDataHome, cognitiveDataHome)) {
        return -1;
    }

    if (envExist("XDG_DATA_HOME")) {
        std::cerr << "XDG_DATA_HOME already exist." << std::endl;
        return -1;
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
        return -1;
    }

    if (envExist("XDG_CONFIG_HOME")) {
        std::cerr << "XDG_CONFIG_HOME already exist." << std::endl;
        return -1;
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
        return -1;
    }

    if (envExist("XDG_CACHE_HOME")) {
        std::cerr << "XDG_CACHE_HOME already exist." << std::endl;
        return -1;
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
        return -1;
    }

    if (envExist("XDG_STATE_HOME")) {
        std::cerr << "XDG_STATE_HOME already exist." << std::endl;
        return -1;
    }
    env.emplace_back("XDG_STATE_HOME=" + cognitiveStateHome.string());

    // systemd user path
    auto hostSystemdUserDir = XDGConfigHome / "systemd" / "user";
    if (std::filesystem::exists(hostSystemdUserDir, ec)) {
        auto cognitiveSystemdUserDir = cognitiveConfigHome / "systemd" / "user";
        if (!mountDir(hostSystemdUserDir, cognitiveSystemdUserDir)) {
            return -1;
        }
    }

    // FIXME: Many applications get configurations from dconf, so we expose dconf to all
    // applications for now. If there is a better solution to fix this issue, please change the
    // following codes
    auto hostUserDconfPath = XDGConfigHome / "dconf";
    if (std::filesystem::exists(hostUserDconfPath, ec)) {
        auto cognitiveAppDconfPath = cognitiveConfigHome / "dconf";
        if (!mountDir(hostUserDconfPath, cognitiveAppDconfPath)) {
            return -1;
        }
    }

    // for dde application theme
    auto hostDDEApiPath = XDGCacheHome / "deepin" / "dde-api";
    if (std::filesystem::exists(hostDDEApiPath, ec)) {
        auto cognitiveDDEApiPath = cognitiveCacheHome / "deepin" / "dde-api";
        if (!mountDir(hostDDEApiPath, cognitiveDDEApiPath)) {
            return -1;
        }
    }

    // for xdg-user-dirs
    auto XDGUserDirs = XDGConfigHome / "user-dirs.dirs";
    if (std::filesystem::exists(XDGUserDirs, ec)) {
        mounts.push_back({
          { "destination", cognitiveConfigHome / "user-dirs.dirs" },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", XDGUserDirs },
          { "type", "bind" },
        });
    }

    auto XDGUserLocale = XDGConfigHome / "user-dirs.locale";
    if (std::filesystem::exists(XDGUserLocale, ec)) {
        mounts.push_back({
          { "destination", cognitiveConfigHome / "user-dirs.locale" },
          { "options", nlohmann::json::array({ "rbind" }) },
          { "source", XDGUserLocale },
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

    // hide self data
    auto linglongDataDir = hostHomeDir / ".linglong";
    if (!mountDir(linglongDataDir / "data", cognitiveHomeDir / ".linglong")) {
        return -1;
    }

    auto privileges = linglong::api::types::v1::ApplicationAccessPrivileges{};
    auto config = privateAppDir / "permissions.json";
    if (!std::filesystem::exists(config, ec)) {
        if (ec) {
            std::cerr << "failed to get status of " << config.c_str() << ": " << ec.message()
                      << std::endl;
            return -1;
        }

        // no permission config, do nothing
        std::cout << content.dump() << std::endl;
        return 0;
    }

    auto input = std::ifstream(config);
    if (!input.is_open()) {
        std::cerr << "couldn't open config file " << config.c_str() << std::endl;
        return -1;
    }

    try {
        auto content = nlohmann::json::parse(input);
        privileges = content.get<linglong::api::types::v1::ApplicationAccessPrivileges>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "deserialize error:" << e.what() << std::endl;
        return -1;
    } catch (std::exception &e) {
        std::cerr << "unknown exception:" << e.what() << std::endl;
        return -1;
    }

    auto directories =
      privileges.userDirectories.value_or(linglong::api::types::v1::UserDirectories{});

    // FIXME: we should resolve real home through env GNUPGHOME
    // FIXME: we should resolve user dirs through ${XDG_CONFIG_HOME}/user-dirs.dirs

    // process blocklist
    static const std::unordered_set<std::string_view> blackList = { ".gnupg", ".ssh" };
    auto disallowedDirs = directories.disallowed.value_or(std::vector<std::string>{});
    disallowedDirs.insert(disallowedDirs.end(), blackList.begin(), blackList.end());
    std::sort(disallowedDirs.begin(), disallowedDirs.end());
    auto dupIt = std::unique(disallowedDirs.begin(), disallowedDirs.end());
    disallowedDirs.erase(dupIt, disallowedDirs.end());

    for (const std::filesystem::path relative : disallowedDirs) {
        if (relative.empty() || relative.is_absolute()) {
            std::cerr << "invalid path:" << relative << std::endl;
            return -1;
        }

        if (auto hostLocation = hostHomeDir / relative;
            !std::filesystem::exists(hostLocation, ec)) {
            if (ec) {
                std::cerr << "failed to get state of " << hostLocation << ": " << ec.message()
                          << std::endl;
                return -1;
            }

            continue;
        }

        // we don't need to concern about source is symlink
        if (!mountDir(privateAppDir / relative, cognitiveHomeDir / relative)) {
            return -1;
        }
    }

    // process whitelist
    auto allowedDirs = directories.allowed.value_or(std::vector<std::string>{});
    std::sort(allowedDirs.begin(), allowedDirs.end());
    dupIt = std::unique(allowedDirs.begin(), allowedDirs.end());
    allowedDirs.erase(dupIt, allowedDirs.end());
    for (std::filesystem::path relative : allowedDirs) {
        if (relative.empty() || relative.is_absolute()) {
            std::cerr << "invalid path:" << relative << std::endl;
            return -1;
        }

        if (blackList.find(relative.string()) != blackList.end()) {
            continue;
        }

        auto hostLocation = hostHomeDir / relative;
        std::filesystem::file_status status = std::filesystem::symlink_status(hostLocation, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) {
                continue;
            }

            std::cerr << "failed to get file type of" << hostLocation << ": " << ec.message()
                      << std::endl;
            return -1;
        }

        if (status.type() != std::filesystem::file_type::symlink) {
            if (!mountDir(hostLocation, cognitiveHomeDir / relative)) {
                return -1;
            }

            continue;
        }

        std::array<char, PATH_MAX + 1> buf{};
        auto *resolved = ::realpath(hostLocation.c_str(), buf.data());
        if (resolved == nullptr) {
            std::cerr << "failed to resolve symlink " << hostLocation << ": " << ::strerror(errno)
                      << std::endl;
            return -1;
        }

        auto realHostLocation = std::filesystem::path(resolved);
        if (!std::filesystem::exists(realHostLocation, ec)) {
            if (ec) {
                std::cerr << "failed to get state of " << realHostLocation << ": " << ec.message()
                          << std::endl;
                return -1;
            }

            continue;
        }

        if (auto realHostLocationStr = realHostLocation.string();
            realHostLocationStr.rfind(hostHomeDir, 0) != 0) {
            std::cerr << "real host directory doesn't in user's home:" << realHostLocation
                      << std::endl;
            return -1;
        }

        mounts.push_back({
          { "destination", hostLocation.string() },
          { "options", nlohmann::json::array({ "rbind", "copy-symlink" }) },
          { "source", hostLocation.string() },
          { "type", "bind" },
        });
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
