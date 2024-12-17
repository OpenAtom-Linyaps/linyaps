// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/oci-cfg-generators/builtins.h"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "ocppi/runtime/config/types/Hook.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static std::atomic_bool bundleFlag{ false };
static std::filesystem::path containerBundle;

template<typename Func>
struct defer
{
    explicit defer(Func newF)
        : f(std::move(newF))
    {
    }

    ~defer() { f(); }

private:
    Func f;
};

std::string genRandomString() noexcept
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<char> dist('A', 'z');

    std::string ret;
    while (ret.size() < 81) {
        auto val = dist(gen);
        if (val >= '[' && val <= '`') {
            continue;
        }
        ret.push_back(val);
    }

    return ret;
}

void cleanResource()
{
    if (!bundleFlag.load(std::memory_order_relaxed)) {
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(containerBundle, ec) || ec) {
        std::cerr << "filesystem error:" << containerBundle << " " << ec.message() << std::endl;
        return;
    }

    if (!std::filesystem::remove_all(containerBundle, ec) || ec) {
        std::cerr << "failed to remove directory " << containerBundle << ":" << ec.message()
                  << std::endl;
        return;
    }

    bundleFlag.store(false, std::memory_order_relaxed);
}

[[noreturn]] static void cleanAndExit(int exitCode) noexcept
{
    cleanResource();
    ::_exit(exitCode);
}

void handleSig() noexcept
{
    auto handler = [](int sig) -> void {
        cleanAndExit(sig);
    };

    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    auto quitSignals = { SIGTERM, SIGINT, SIGQUIT, SIGHUP, SIGABRT, SIGSEGV };
    for (auto sig : quitSignals) {
        sigaddset(&blocking_mask, sig);
    }

    struct sigaction sa
    {
    };

    sa.sa_handler = handler;
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals) {
        sigaction(sig, &sa, nullptr);
    }
}

void applyPatches(ocppi::runtime::config::types::Config &config) noexcept
{
    const auto &builtins = linglong::generator::builtin_generators();
    for (const auto &[name, gen] : builtins) {
        if (!gen->generate(config)) {
            std::cerr << "generator " << name << " failed" << std::endl;
        }
    }
}

std::optional<linglong::api::types::v1::PackageInfoV2>
loadPackageInfoFromJson(const std::filesystem::path &json) noexcept
{
    std::ifstream stream{ json };
    if (!stream.is_open()) {
        std::cerr << "couldn't open " << json << std::endl;
        return std::nullopt;
    }

    try {
        auto content = nlohmann::json::parse(stream);
        return content.get<linglong::api::types::v1::PackageInfoV2>();
    } catch (const std::exception &err) {
        std::cout << "catching exception: " << err.what() << std::endl;
    }

    std::cout << "fallback to PackageInfoV1" << std::endl;
    stream.seekg(0);

    try { // TODO: use public fallback method on later
        auto content = nlohmann::json::parse(stream);
        auto oldInfo = content.get<linglong::api::types::v1::PackageInfo>();
        return linglong::api::types::v1::PackageInfoV2{
            .arch = oldInfo.arch,
            .base = oldInfo.base,
            .channel = oldInfo.channel.value_or("main"),
            .command = oldInfo.command,
            .description = oldInfo.description,
            .id = oldInfo.appid,
            .kind = oldInfo.kind,
            .packageInfoV2Module = oldInfo.packageInfoModule,
            .name = oldInfo.name,
            .permissions = oldInfo.permissions,
            .runtime = oldInfo.runtime,
            .schemaVersion = "1.0",
            .size = oldInfo.size,
            .version = oldInfo.version,
        };
    } catch (const nlohmann::json::parse_error &err) {
        std::cerr << "parsing error: " << err.what() << std::endl;
    } catch (const nlohmann::json::out_of_range &err) {
        std::cerr << "parsing out of range:" << err.what() << std::endl;
    } catch (const std::exception &err) {
        std::cout << "catching exception: " << err.what() << std::endl;
    }

    return std::nullopt;
}

// DO NOT USE LOADER DIRECTLY
int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) // NOLINT
{
    handleSig();

    // This 'bundle' means uab bundle instead of OCI bundle
    const auto &bundleDir = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    const auto &layersDir = bundleDir / "layers";

    std::error_code ec;
    if (!std::filesystem::exists(layersDir, ec) || ec) {
        std::cerr << "couldn't find directory 'layers', maybe filesystem error:" << ec.message()
                  << std::endl;
        return -1;
    }

    std::optional<linglong::api::types::v1::PackageInfoV2> appInfo;
    for (const auto &layer : std::filesystem::directory_iterator{ layersDir, ec }) {
        if (!layer.is_directory()) { // seeking layer directory
            continue;
        }

        bool foundApp{ false };
        for (const auto &layerModule : std::filesystem::directory_iterator{ layer, ec }) {
            if (!layerModule.is_directory()
                || layerModule.path().filename()
                  != "binary") { // we only need binary module to run the application
                continue;
            }

            auto infoFile = layerModule.path() / "info.json";
            auto info = loadPackageInfoFromJson(infoFile);
            if (!info) {
                std::cerr << "couldn't get meta info, exit" << std::endl;
                return -1;
            }

            if (info->kind == "app") {
                appInfo = std::move(info);
                foundApp = true;
                break;
            }
        }

        if (ec) {
            std::cerr << "uab internal module error: " << ec.message() << " code:" << ec.value()
                      << std::endl;
            return -1;
        }

        if (foundApp) {
            break;
        }
    }

    if (ec) {
        std::cerr << "uab internal layer error: " << ec.message() << " code:" << ec.value()
                  << std::endl;
        return -1;
    }

    if (!appInfo) {
        std::cerr << "couldn't find meta info of application" << std::endl;
        return -1;
    }

    std::string appID = appInfo->id;
    const auto &baseStr = appInfo->base;
    auto splitSlash = std::find(baseStr.cbegin(), baseStr.cend(), '/');
    auto splitColon = std::find(baseStr.cbegin(), baseStr.cend(), ':');
    auto baseID =
      baseStr.substr(std::distance(baseStr.cbegin(), splitColon) + 1, splitSlash - splitColon - 1);

    std::string runtimeID;
    if (appInfo->runtime) {
        const auto &runtimeStr = appInfo->runtime.value();
        auto splitSlash = std::find(runtimeStr.cbegin(), runtimeStr.cend(), '/');
        auto splitColon = std::find(runtimeStr.cbegin(), runtimeStr.cend(), ':');
        runtimeID = runtimeStr.substr(std::distance(runtimeStr.cbegin(), splitColon) + 1,
                                      splitSlash - splitColon - 1);
    }

    auto containerID = genRandomString();
    auto containerBundleDir = bundleDir.parent_path().parent_path() / containerID;
    if (!std::filesystem::create_directories(containerBundleDir, ec) || ec) {
        std::cerr << "couldn't create directory " << containerBundleDir << " :" << ec.message()
                  << std::endl;
        return -1;
    }

    containerBundle = std::move(containerBundleDir);
    bundleFlag.store(true, std::memory_order_relaxed);
    defer removeContainerBundleDir{ cleanResource };

    auto extraDir = bundleDir / "extra";
    if (!std::filesystem::exists(extraDir, ec) || ec) {
        std::cerr << extraDir << " may not exist:" << ec.message() << std::endl;
        return -1;
    }

    auto boxBin = extraDir / "ll-box";
    if (!std::filesystem::exists(boxBin, ec) || ec) {
        std::cerr << boxBin << " may not exist:" << ec.message() << std::endl;
        return -1;
    }

    ocppi::runtime::config::types::Config config;
    try {
        auto content = nlohmann::json::parse(linglong::generator::initConfig);
        config = content.get<ocppi::runtime::config::types::Config>();
    } catch (std::exception &e) {
        std::cerr << "catch an exception:" << e.what() << std::endl;
        return -1;
    }

    auto compatibleFilePath = [&bundleDir](std::string_view layerID) -> std::string {
        std::error_code ec;

        auto layerDir = bundleDir / "layers" / layerID;
        if (!std::filesystem::exists(layerDir, ec) || ec) {
            std::cerr << "container layer path: " << layerDir << "may not exist:" << ec.message()
                      << std::endl;
            return {};
        }

        // ignore error code when directory doesn't exist
        if (auto runtime = layerDir / "runtime/files"; std::filesystem::exists(runtime, ec)) {
            return runtime.string();
        }

        if (auto binary = layerDir / "binary/files"; std::filesystem::exists(binary, ec)) {
            return binary.string();
        }

        std::cerr << "layer runtime doesn't exist." << std::endl;
        return {};
    };

    auto layerFilesDir = compatibleFilePath(baseID);
    if (layerFilesDir.empty()) {
        std::cerr << "couldn't get compatiblePath of base" << std::endl;
        return -1;
    }

    config.root->path = std::move(layerFilesDir);
    config.root->readonly = true;

    auto annotations = config.annotations.value_or(std::map<std::string, std::string>{});
    annotations["org.deepin.linglong.appID"] = appID;
    if (!runtimeID.empty()) {
        layerFilesDir = compatibleFilePath(runtimeID);
        if (layerFilesDir.empty()) {
            std::cerr << "couldn't get compatiblePath of runtime" << std::endl;
            return -1;
        }

        annotations["org.deepin.linglong.runtimeDir"] =
          std::filesystem::path{ layerFilesDir }.parent_path();
    }

    layerFilesDir = compatibleFilePath(appID);
    if (layerFilesDir.empty()) {
        std::cerr << "couldn't get compatiblePath of application" << std::endl;
        return -1;
    }

    auto appDir = std::filesystem::path{ layerFilesDir }.parent_path();

    annotations["org.deepin.linglong.appDir"] = appDir.string();
    annotations["org.deepin.linglong.bundleDir"] = containerBundle.string();
    config.annotations = std::move(annotations);

    // replace commands
    if (!appInfo->command || appInfo->command->empty()) {
        std::cerr << "couldn't find command of application" << std::endl;
        return -1;
    }
    auto command = appInfo->command.value();

    // To avoid the original args containing spaces, each arg is wrapped in single quotes, and the
    // single quotes inside the arg are escaped and replaced
    using namespace std::literals;
    constexpr auto replaceStr = R"('\'')"sv;
    std::for_each(command.begin(), command.end(), [replaceStr](std::string &arg) {
        std::string::size_type pos{ 0 };
        while ((pos = arg.find('\'', pos)) != std::string::npos) {
            arg.replace(pos, replaceStr.size(), replaceStr);
            pos += replaceStr.size();
        }

        arg.insert(arg.begin(), '\'');
        arg.push_back('\'');
    });
    command.insert(command.begin(), "exec");

    std::string commandStr;
    while (true) {
        commandStr.append(command.front());
        command.erase(command.begin());

        if (command.empty()) {
            break;
        }

        commandStr.push_back(' ');
    }

    // Add 'bash' '--login' '-c' in front of the original args so that we can use the environment
    // variables configured in /etc/profile
    command = { "/bin/bash", "--login", "-c", std::move(commandStr) };
    config.process->args = std::move(command);

    applyPatches(config);

    // append ld conf
    auto ldConfDir = extraDir / "ld.conf.d";
    if (!std::filesystem::exists(ldConfDir, ec) || ec) {
        std::cerr << "ld config directory may not exist" << std::endl;
        return -1;
    }

    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
      .options = { { "ro", "rbind" } },
      .source = ldConfDir / "zz_deepin-linglong-app.ld.so.conf",
      .type = "bind",
    });

    // generate ld.so.cache and font cache at runtime
    auto cacheDir = containerBundle / "cache";
    auto ldCacheDir = cacheDir / "ld.so.cache.d";
    auto fontCacheDir = cacheDir / "fontconfig";
    if (!std::filesystem::create_directories(ldCacheDir, ec)) {
        std::cerr << "failed to create ld cache directory:" << ec.message() << std::endl;
        return -1;
    }

    if (!std::filesystem::create_directories(fontCacheDir, ec)) {
        std::cerr << "failed to create font cache directory:" << ec.message() << std::endl;
        return -1;
    }

    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/cache",
      .options = { { "rbind", "rw" } },
      .source = ldCacheDir,
      .type = "bind",
    });

    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/var/cache/fontconfig",
      .options = { { "rbind", "rw" } },
      .source = fontCacheDir,
      .type = "bind",
    });

    auto hooks = config.hooks.value_or(ocppi::runtime::config::types::Hooks{});
    auto startHooks =
      hooks.startContainer.value_or(std::vector<ocppi::runtime::config::types::Hook>{});
    startHooks.push_back(ocppi::runtime::config::types::Hook{
      .args = std::vector<std::string>{ "/sbin/ldconfig", "-C", "/run/linglong/cache/ld.so.cache" },
      .env = {},
      .path = "/sbin/ldconfig",
      .timeout = {},
    });
    startHooks.push_back(ocppi::runtime::config::types::Hook{
      .args = std::vector<std::string>{ "fc-cache", "-f" },
      .env = {},
      .path = "/bin/fc-cache",
      .timeout = {},
    });
    hooks.startContainer = std::move(startHooks);
    config.hooks = std::move(hooks);

    // dump to bundle
    auto bundleCfg = containerBundle / "config.json";
    std::ofstream cfgStream{ bundleCfg.string() };
    if (!cfgStream.is_open()) {
        std::cerr << "couldn't create bundle config.json" << std::endl;
        return -1;
    }

    nlohmann::json json = config;
    cfgStream << json.dump() << std::endl;
    cfgStream.close();

    if (::getenv("LINGLONG_UAB_DEBUG") != nullptr) {
        std::cout << "dump container:" << std::endl;
        std::cout << json.dump(4) << std::endl;
    }

    auto bundleArg = "--bundle=" + containerBundle.string();
    auto pid = fork();
    if (pid < 0) {
        std::cerr << "fork() err: " << ::strerror(errno) << std::endl;
        return -1;
    }

    if (pid == 0) {
        return ::execl(boxBin.c_str(),
                       boxBin.c_str(),
                       "--cgroup-manager=disabled",
                       "run",
                       bundleArg.c_str(),
                       "--config=config.json",
                       containerID.c_str(),
                       nullptr);
    }

    int wstatus{ -1 };
    if (auto ret = ::waitpid(pid, &wstatus, 0); ret == -1) {
        std::cerr << "waitpid() err:" << ::strerror(errno) << std::endl;
        return -1;
    }

    return WEXITSTATUS(wstatus);
}
