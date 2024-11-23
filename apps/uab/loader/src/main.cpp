// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <utility>

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
    return;
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

bool applyJSONPatch(ocppi::runtime::config::types::Config &cfg,
                    const linglong::api::types::v1::OciConfigurationPatch &patch) noexcept
{
    if (patch.ociVersion != cfg.ociVersion) {
        std::cerr << "ociVersion mismatched" << std::endl;
        return false;
    }

    auto raw = nlohmann::json(cfg);
    try {
        raw = raw.patch(patch.patch);
        cfg = raw.get<ocppi::runtime::config::types::Config>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "patch parser error:" << e.what() << std::endl;
        return false;
    } catch (nlohmann::json::out_of_range &e) {
        std::cerr << "patch out of range:" << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "unknown exception" << std::endl;
        return false;
    }

    return true;
}

void applyJSONFilePatch(ocppi::runtime::config::types::Config &cfg,
                        const std::filesystem::path &info) noexcept
{
    std::string_view suffix{ ".json" };
    auto fileName = info.filename().string();
    if (fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) != 0) {
        std::cerr << "file not ends with " << suffix << std::endl;
        return;
    }

    std::ifstream stream{ info };
    if (!stream.is_open()) {
        std::cerr << "couldn't open file " << info << std::endl;
        return;
    }

    linglong::api::types::v1::OciConfigurationPatch patch;
    try {
        auto content = nlohmann::json::parse(stream);
        patch = content.get<linglong::api::types::v1::OciConfigurationPatch>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "parse json file(" << info << ") error:" << e.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "unknown exception" << std::endl;
        return;
    }

    applyJSONPatch(cfg, patch);
}

void applyExecutablePatch(const std::filesystem::path &workdir,
                          ocppi::runtime::config::types::Config &cfg,
                          const std::filesystem::path &info) noexcept
{
    std::array<int, 2> inPipe{ -1, -1 };
    std::array<int, 2> outPipe{ -1, -1 };
    std::array<int, 2> errPipe{ -1, -1 };
    if (::pipe(outPipe.data()) == -1) {
        std::cerr << "pipe error:" << ::strerror(errno) << std::endl;
        return;
    }

    auto clearPipe = [](int &fd) {
        ::close(fd);
        fd = -1;
    };

    auto closeInPipe = defer([&pipes = outPipe] {
        for (auto &pipe : pipes) {
            if (pipe != -1) {
                ::close(pipe);
            }
        }
    });

    if (::pipe(errPipe.data()) == -1) {
        std::cerr << "pipe error:" << ::strerror(errno) << std::endl;
        return;
    }

    auto closeErrPipe = defer([&pipes = errPipe] {
        for (auto &pipe : pipes) {
            if (pipe != -1) {
                ::close(pipe);
            }
        }
    });

    if (::pipe(inPipe.data()) == -1) {
        std::cerr << "pipe error:" << ::strerror(errno) << std::endl;
        return;
    }

    auto closeOPipe = defer([&pipes = inPipe] {
        for (auto &pipe : pipes) {
            if (pipe != -1) {
                ::close(pipe);
            }
        }
    });

    auto pid = fork();
    if (pid < 0) {
        std::cerr << "fork error:" << ::strerror(errno) << std::endl;
        return;
    }

    if (pid == 0) {
        std::error_code ec;
        std::filesystem::current_path(workdir, ec);
        if (ec) {
            std::cerr << "change workdir error: " << ec.message() << std::endl;
            return;
        }

        ::dup2(inPipe[0], STDIN_FILENO);
        ::dup2(outPipe[1], STDOUT_FILENO);
        ::dup2(errPipe[1], STDERR_FILENO);

        clearPipe(inPipe[1]);
        clearPipe(errPipe[0]);
        clearPipe(outPipe[0]);

        if (::execl(info.c_str(), info.c_str(), nullptr) == -1) {
            std::cerr << "execl " << info << " error:" << ::strerror(errno) << std::endl;
            return;
        }
    }

    clearPipe(inPipe[0]);
    clearPipe(errPipe[1]);
    clearPipe(outPipe[1]);

    auto input = nlohmann::json(cfg).dump();
    auto bytesWrite = input.size();
    while (true) {
        auto writeBytes = ::write(inPipe[1], input.c_str(), bytesWrite);
        if (writeBytes == -1) {
            if (errno == EINTR) {
                continue;
            }

            return;
        }

        bytesWrite -= writeBytes;
        if (bytesWrite == 0) {
            break;
        }
    }
    clearPipe(inPipe[1]);

    using namespace std::chrono_literals;
    auto timeout = 400;
    int wstatus{ -1 };
    std::string err;
    std::string out;

    while (true) {
        std::this_thread::sleep_for(100ms);
        int child{ -1 };
        if (child = ::waitpid(pid, &wstatus, WNOHANG); child == -1) {
            std::cerr << "wait for process error:" << ::strerror(errno) << std::endl;
            return;
        }

        if (child == 0) {
            timeout -= 100;
            if (timeout == 0) {
                std::cerr << "generator " << info << " timeout" << std::endl;
                return;
            }
            continue;
        }

        std::array<char, 4096> buf{};
        int reads{ -1 };
        while ((reads = ::read(errPipe[0], buf.data(), buf.size())) != 0) {
            if (reads < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "read error:" << strerror(errno) << std::endl;
                return;
            }

            std::copy_n(buf.cbegin(), reads, std::back_inserter(err));
        }

        reads = -1;
        while ((reads = ::read(outPipe[0], buf.data(), buf.size())) != 0) {
            if (reads < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "read error:" << ::strerror(errno) << std::endl;
                return;
            }

            std::copy_n(buf.cbegin(), reads, std::back_inserter(out));
        }

        break;
    }

    auto exitCode = WEXITSTATUS(wstatus);
    if (exitCode != 0) {
        std::cerr << "generator " << info << " return " << exitCode << std::endl;
        std::cerr << "input:" << input << "stderr:" << err << std::endl;
        return;
    }

    if (!err.empty()) {
        std::cerr << "generator " << info << " stderr:" << err << std::endl;
    }

    nlohmann::json modified;
    try {
        auto content = nlohmann::json::parse(out);
        modified = content.get<ocppi::runtime::config::types::Config>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "parse output error: " << e.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "unknown exception occurred during parsing output" << std::endl;
        return;
    }

    clearPipe(errPipe[0]);
    clearPipe(outPipe[0]);

    cfg = std::move(modified);
}

void applyPatches(const std::filesystem::path &woridir,
                  ocppi::runtime::config::types::Config &cfg,
                  const std::vector<std::filesystem::path> &patches) noexcept
{
    auto testExec = [](std::filesystem::perms perm) {
        using p = std::filesystem::perms;
        return (perm & p::owner_exec) != p::none || (perm & p::group_exec) != p::none
          || (perm & p::others_exec) != p::none;
    };

    std::error_code ec;
    for (const auto &info : patches) {
        if (!std::filesystem::is_regular_file(info, ec)) {
            if (ec) {
                std::cerr << "couldn't get file type of " << info << ": " << ec.message()
                          << " ,skip." << std::endl;
                continue;
            }

            std::cerr << info << " isn't a regular file, skip." << std::endl;
            continue;
        }

        auto status = std::filesystem::status(info, ec);
        if (ec) {
            std::cerr << "couldn't get status of " << info << std::endl;
            continue;
        }

        if (testExec(status.permissions())) {
            applyExecutablePatch(woridir, cfg, info);
            continue;
        }

        applyJSONFilePatch(cfg, info);
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
    } catch (const nlohmann::json::parse_error &err) {
        std::cerr << "parsing error: " << err.what() << std::endl;
    } catch (const nlohmann::json::out_of_range &err) {
        std::cerr << "parsing out of range:" << err.what() << std::endl;
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

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    handleSig();

    const auto &bundleDir = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    const auto &layersDir = bundleDir / "layers";
    std::error_code ec;
    if (!std::filesystem::exists(layersDir, ec) || ec) {
        std::cerr << "couldn't find directory 'layers':" << ec.message() << std::endl;
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

    auto containerCfgDir = extraDir / "container";
    if (!std::filesystem::exists(containerCfgDir, ec) || ec) {
        std::cerr << containerCfgDir << " may not exist:" << ec.message() << std::endl;
        return -1;
    }

    auto initCfg = containerCfgDir / "config.json";
    if (!std::filesystem::exists(initCfg, ec) || ec) {
        std::cerr << initCfg << " may not exist:" << ec.message() << std::endl;
        return -1;
    }

    std::ifstream stream{ initCfg };
    if (!stream.is_open()) {
        std::cerr << "open container config error." << std::endl;
        return -1;
    }

    ocppi::runtime::config::types::Config config;
    try {
        auto content = nlohmann::json::parse(stream);
        config = content.get<ocppi::runtime::config::types::Config>();
    } catch (nlohmann::json::parse_error &e) {
        std::cerr << "parse container config failed: " << e.what() << std::endl;
        return -1;
    } catch (std::exception &e) {
        std::cerr << "catch an exception:" << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "catch an unknown exception." << std::endl;
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

    // apply generators
    auto generatorsDir = containerCfgDir / "config.d";
    if (!std::filesystem::exists(generatorsDir, ec) || ec) {
        std::cerr << initCfg << " may not exist:" << ec.message() << std::endl;
        return -1;
    }

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

    std::filesystem::directory_iterator it{ generatorsDir, ec };
    if (ec) {
        std::cerr << "construct directory iterator error:" << ec.message() << std::endl;
        return -1;
    }

    std::vector<std::filesystem::path> gens;
    for (const auto &path : it) {
        gens.emplace_back(path.path());
    }

    applyPatches(containerBundle, config, gens);

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
