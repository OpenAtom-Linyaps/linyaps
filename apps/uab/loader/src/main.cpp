// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "ocppi/runtime/config/types/Generators.hpp" // IWYU pragma: keep
#include "ocppi/runtime/config/types/Hook.hpp"
#include "ocppi/runtime/config/types/Linux.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::filesystem::path containerBundle;

std::string genRandomString() noexcept
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<char> dist('A', 'z');

    std::string ret;
    while (ret.size() < 16) {
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
    if (containerBundle.empty()) {
        return;
    }

    containerBundle.clear();
    if (::getenv("LINGLONG_UAB_DEBUG") != nullptr) {
        return;
    }

    std::error_code ec;
    if (std::filesystem::remove_all(containerBundle, ec) == static_cast<std::uintmax_t>(-1) && ec) {
        std::cerr << "failed to remove directory " << containerBundle << ":" << ec.message()
                  << std::endl;
        return;
    }
}

[[noreturn]] void cleanAndExit(int exitCode) noexcept
{
    cleanResource();
    ::_exit(exitCode);
}

void handleSig() noexcept
{
    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);
    auto quitSignals = { SIGTERM, SIGINT, SIGQUIT, SIGHUP, SIGABRT };
    for (auto sig : quitSignals) {
        sigaddset(&blocking_mask, sig);
    }

    struct sigaction sa{};

    sa.sa_handler = [](int sig) -> void {
        cleanAndExit(128 + sig);
    };
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals) {
        sigaction(sig, &sa, nullptr);
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

bool processLDConfig(linglong::generator::ContainerCfgBuilder &builder,
                     const std::string &arch) noexcept
{
    std::optional<std::string> triplet;
    if (arch == "x86_64") {
        triplet = "x86_64-linux-gnu";
    } else if (arch == "arm64") {
        triplet = "aarch64-linux-gnu";
    } else if (arch == "loong64" || arch == "loongarch64") {
        triplet = "loongarch64-linux-gnu";
    } else if (arch == "sw64") {
        triplet = "sw_64-linux-gnu";
    } else if (arch == "mips64") {
        triplet = "mips64el-linux-gnuabi64";
    }

    if (!triplet) {
        std::cerr << "unsupported architecture" << std::endl;
        return false;
    }

    auto content = builder.ldConf(triplet.value());
    auto ldConf = builder.getBundlePath() / "ld.so.conf";
    {
        std::ofstream stream{ ldConf };
        if (!stream.is_open()) {
            std::cout << "failed to open " << ldConf.string() << std::endl;
            return false;
        }

        stream << content;
    }

    // trigger fixMount
    auto randomFile = builder.getBundlePath() / genRandomString();
    {
        std::ofstream stream{ randomFile };
        if (!stream.is_open()) {
            std::cout << "failed to open " << ldConf.string() << std::endl;
            return false;
        }
    }

    builder.addExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
      ocppi::runtime::config::types::Mount{
        .destination = "/etc/" + randomFile.filename().string(),
        .options = { { "ro", "rbind" } },
        .source = randomFile,
        .type = "bind",
      },
      ocppi::runtime::config::types::Mount{
        .destination = "/etc/ld.so.conf.d/zz_deepin-linglong.ld.so.conf",
        .options = { { "ro", "rbind" } },
        .source = ldConf,
        .type = "bind",
      } });

    builder.setStartContainerHooks(std::vector<ocppi::runtime::config::types::Hook>{
      ocppi::runtime::config::types::Hook{
        .args = std::vector<std::string>{ "/sbin/ldconfig", "-C", "/tmp/ld.so.cache" },
        .path = "/sbin/ldconfig",
      },
      ocppi::runtime::config::types::Hook{
        .args =
          std::vector<std::string>{ "/bin/sh", "-c", "cat /tmp/ld.so.cache > /etc/ld.so.cache" },
        .path = "/bin/sh",
      } });

    return true;
}

} // namespace

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

    if (appInfo->base.empty()) {
        std::cerr << "couldn't find base of application" << std::endl;
        return -1;
    }

    auto containerID = genRandomString();
    auto containerBundleDir = bundleDir.parent_path().parent_path() / containerID;
    if (!std::filesystem::create_directories(containerBundleDir, ec) && ec) {
        std::cerr << "couldn't create directory " << containerBundleDir << " :" << ec.message()
                  << std::endl;
        return -1;
    }

    containerBundle = std::move(containerBundleDir);

    if (std::atexit(cleanResource) != 0) {
        std::cerr << "failed register exit handler" << std::endl;
        return 1;
    }

    std::set_terminate([]() {
        cleanResource();
        std::abort();
    });

    auto uid = ::getuid();
    auto gid = ::getgid();
    linglong::generator::ContainerCfgBuilder builder;

    auto runtimeLD = containerBundle / "ld.so.cache";
    {
        std::ofstream stream{ runtimeLD };
        if (!stream) {
            std::cerr << "failed to open file " << runtimeLD << std::endl;
            return -1;
        }
    }

    builder.setBundlePath(containerBundle)
      .setBasePath("/", false)
      .enableSelfAdjustingMount()
      .forwardEnv()
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .addExtraMounts(
        std::vector<ocppi::runtime::config::types::Mount>{ ocppi::runtime::config::types::Mount{
                                                             .destination = "/etc/ld.so.cache",
                                                             .options = { { "rbind" } },
                                                             .source = runtimeLD,
                                                             .type = "bind",
                                                           },
                                                           ocppi::runtime::config::types::Mount{
                                                             .destination = "/tmp",
                                                             .options = { { "rbind" } },
                                                             .source = "/tmp",
                                                             .type = "bind",
                                                           } });

    auto extraDir = bundleDir / "extra";
    if (!std::filesystem::exists(extraDir, ec)) {
        if (ec) {
            std::cerr << "failed to get directory " << extraDir << ":" << ec.message()
                      << " code:" << ec.value() << std::endl;
            return -1;
        }

        std::cerr << extraDir << " not exist." << std::endl;
        return -1;
    }

    auto boxBin = extraDir / "ll-box";
    if (!std::filesystem::exists(boxBin, ec)) {
        if (ec) {
            std::cerr << "failed to get file " << boxBin << ":" << ec.message()
                      << " code:" << ec.value() << std::endl;
            return -1;
        }

        std::cerr << boxBin << " not exist." << std::endl;
        return -1;
    }

    auto compatibleFilePath = [&bundleDir](std::string_view layerID) -> std::filesystem::path {
        std::error_code ec;
        auto layerDir = bundleDir / "layers" / layerID;
        if (!std::filesystem::exists(layerDir, ec)) {
            if (ec) {
                std::cerr << "get layer directory " << layerDir << " error:" << ec.message()
                          << " code:" << ec.value() << std::endl;
                return {};
            }

            std::cerr << layerDir << " not exist." << std::endl;
            return {};
        }

        // ignore error code when directory doesn't exist
        if (auto runtime = layerDir / "runtime" / "files"; std::filesystem::exists(runtime, ec)) {
            return runtime.string();
        }

        if (auto binary = layerDir / "binary" / "files"; std::filesystem::exists(binary, ec)) {
            return binary.string();
        }

        std::cerr << "layer runtime doesn't exist." << std::endl;
        return {};
    };

    auto rootfs = containerBundle / "rootfs";
    if (!std::filesystem::create_directories(rootfs, ec) && ec) {
        std::cerr << "couldn't create directory " << rootfs << " :" << ec.message() << std::endl;
        return -1;
    }

    const auto &appID = appInfo->id;
    builder.setAppId(appID);

    std::string runtimeID;
    if (appInfo->runtime) {
        const auto &runtimeStr = appInfo->runtime.value();

        std::size_t begin{ 0 };
        auto splitColon = std::find(runtimeStr.cbegin(), runtimeStr.cend(), ':');
        if (splitColon != runtimeStr.cend()) {
            begin = std::distance(runtimeStr.cbegin(), splitColon) + 1;
        }

        auto splitSlash = std::find(runtimeStr.cbegin(), runtimeStr.cend(), '/');
        auto len = std::distance(runtimeStr.cbegin(), splitSlash) - begin;

        if (begin + len > runtimeStr.size()) {
            std::cerr << "runtime may not valid: " << runtimeStr << std::endl;
            return -1;
        }

        runtimeID = runtimeStr.substr(begin, len);
    }

    std::filesystem::path runtimeLayerFilesDir;
    if (!runtimeID.empty()) {
        runtimeLayerFilesDir = compatibleFilePath(runtimeID);
        if (runtimeLayerFilesDir.empty()) {
            std::cerr << "couldn't get compatiblePath of runtime" << std::endl;
            return -1;
        }

        builder.setRuntimePath(runtimeLayerFilesDir);
    }

    auto appLayerFilesDir = compatibleFilePath(appID);
    if (appLayerFilesDir.empty()) {
        std::cerr << "couldn't get compatiblePath of application" << std::endl;
        return -1;
    }
    builder.setAppPath(appLayerFilesDir);

    // generate ld.so.cache and font cache at runtime
    if (!processLDConfig(builder, appInfo->arch[0])) {
        std::cerr << "failed to processing ld config" << std::endl;
        return -1;
    }

    // dump to bundle
    auto bundleCfg = containerBundle / "config.json";
    nlohmann::json json;
    {
        std::ofstream cfgStream{ bundleCfg.string() };
        if (!cfgStream.is_open()) {
            std::cerr << "couldn't create bundle config.json" << std::endl;
            return -1;
        }

        if (!builder.build()) {
            std::cerr << "failed to generate OCI config:" << builder.getError().reason << std::endl;
            return -1;
        }

        // for only-App, we need adjust some config
        json = builder.getConfig();
        auto process = json["process"].get<ocppi::runtime::config::types::Process>();
        process.terminal = (::isatty(STDOUT_FILENO) == 1);
        process.user = ocppi::runtime::config::types::User{ .gid = getgid(), .uid = getuid() };
        process.args = appInfo->command.value_or(std::vector<std::string>{ "bash" });
        json["process"] = std::move(process);

        auto linux_ = json["linux"].get<ocppi::runtime::config::types::Linux>();
        linux_.namespaces = std::vector<ocppi::runtime::config::types::NamespaceReference>{
            ocppi::runtime::config::types::NamespaceReference{
              .type = ocppi::runtime::config::types::NamespaceType::User },
            ocppi::runtime::config::types::NamespaceReference{
              .type = ocppi::runtime::config::types::NamespaceType::Mount }
        };
        json["linux"] = std::move(linux_);

        cfgStream << json.dump() << std::endl;
        cfgStream.close();
    }

    if (::getenv("LINGLONG_UAB_DEBUG") != nullptr) {
        std::cout << "dump container config:" << std::endl;
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

    int wstatus{ 0 };
    if (auto ret = ::waitpid(pid, &wstatus, 0); ret == -1) {
        std::cerr << "waitpid() err:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (WIFEXITED(wstatus)) {
        std::cerr << "loader: container exit: " << WEXITSTATUS(wstatus) << std::endl;
        return WEXITSTATUS(wstatus);
    }

    if (WIFSIGNALED(wstatus)) {
        std::cerr << "loader: container exit with signal: " << WTERMSIG(wstatus) << std::endl;
        return WTERMSIG(wstatus) + 128;
    }

    std::cerr << "unknow exit status" << std::endl;
    return -1;
}
