// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <nlohmann/json.hpp>

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

void applyExecutablePatch(ocppi::runtime::config::types::Config &cfg,
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

void applyPatches(ocppi::runtime::config::types::Config &cfg,
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
            applyExecutablePatch(cfg, info);
            continue;
        }

        applyJSONFilePatch(cfg, info);
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    auto *runtimeID = ::getenv("UAB_RUNTIME_ID");

    auto *baseID = ::getenv("UAB_BASE_ID");
    if (baseID == nullptr) {
        std::cerr << "couldn't get UAB_BASE_ID" << std::endl;
        return -1;
    }

    auto *appID = ::getenv("UAB_APP_ID");
    if (appID == nullptr) {
        std::cerr << "couldn't get UAB_APP_ID" << std::endl;
        return -1;
    }

    std::error_code ec;
    auto containerID = genRandomString();
    auto bundleDir = std::filesystem::current_path().parent_path().parent_path() / containerID;
    if (!std::filesystem::create_directories(bundleDir, ec)) {
        std::cerr << "couldn't create directory " << bundleDir << " :" << ec.message() << std::endl;
        return -1;
    }

    defer removeBundle{ [bundleDir]() noexcept {
        std::error_code ec;
        std::filesystem::remove_all(bundleDir, ec);
        if (ec) {
            std::cerr << "remove bundle error:" << ec.message() << std::endl;
        }
    } };

    auto extraDir = std::filesystem::current_path() / "extra";
    if (!std::filesystem::exists(extraDir, ec)) {
        std::cerr << extraDir << " doesn't exist:" << ec.message() << std::endl;
        return -1;
    }

    auto boxBin = extraDir / "ll-box";
    if (!std::filesystem::exists(boxBin, ec)) {
        std::cerr << boxBin << " doesn't exist:" << ec.message() << std::endl;
        return -1;
    }

    auto containerCfgDir = extraDir / "container";
    if (!std::filesystem::exists(containerCfgDir, ec)) {
        std::cerr << containerCfgDir << " doesn't exist:" << ec.message() << std::endl;
        return -1;
    }

    auto initCfg = containerCfgDir / "config.json";
    if (!std::filesystem::exists(initCfg, ec)) {
        std::cerr << initCfg << " doesn't exist:" << ec.message() << std::endl;
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

    auto compatibleFilePath = [](std::string_view layerID) -> std::string {
        std::error_code ec;

        auto layerDir = std::filesystem::current_path() / "layers" / layerID;
        if (!std::filesystem::exists(layerDir, ec)) {
            std::cerr << "container layer path: " << layerDir << "doesn't exist:" << ec.message()
                      << std::endl;
            return {};
        }

        if (auto runtime = layerDir / "runtime/files"; std::filesystem::exists(runtime)) {
            return runtime.string();
        }

        if (auto binary = layerDir / "binary/files"; std::filesystem::exists(binary)) {
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
    if (!std::filesystem::exists(generatorsDir, ec)) {
        std::cerr << initCfg << " doesn't exist:" << ec.message() << std::endl;
        return -1;
    }

    auto annotations = config.annotations.value_or(std::map<std::string, std::string>{});
    annotations["org.deepin.linglong.appID"] = appID;

    if (runtimeID != nullptr) {
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
    auto appInfo = appDir / "info.json";
    std::ifstream appStream{ appInfo };
    if (!appStream.is_open()) {
        std::cerr << "couldn't open app info.json" << std::endl;
        return -1;
    }

    std::vector<std::string> command;
    try {
        auto content = nlohmann::json::parse(appStream);
        if (content.find("command") != content.end()) {
            command = content["command"].get<std::vector<std::string>>();
        }
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

    if (!command.empty()) {
        config.process->args = command;
    }

    std::filesystem::directory_iterator it{ generatorsDir, ec };
    if (ec) {
        std::cerr << "construct directory iterator error:" << ec.message() << std::endl;
        return -1;
    }

    std::vector<std::filesystem::path> gens;
    for (const auto &path : it) {
        gens.emplace_back(path.path());
    }

    applyPatches(config, gens);

    // append ld conf
    auto ldConfDir = extraDir / "ld.conf.d";
    if (!std::filesystem::exists(ldConfDir)) {
        std::cerr << "ld config directory doesn't exist" << std::endl;
        return -1;
    }

    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
      .options = { { "ro", "rbind" } },
      .source = ldConfDir / "zz_deepin-linglong-app.ld.so.conf",
      .type = "bind",
    });

    {
        std::ofstream ofs(bundleDir / "ld.so.cache");
        if (!ofs.is_open()) {
            std::cerr << "create ld config in bundle directory" << std::endl;
            return -1;
        }
        ofs.close();
    }
    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.cache",
      .options = { { "rbind" } },
      .source = bundleDir / "ld.so.cache",
      .type = "bind",
    });

    {
        std::ofstream ofs(bundleDir / "ld.so.cache~");
        if (!ofs.is_open()) {
            std::cerr << "create ld config in bundle directory" << std::endl;
            return -1;
        }
        ofs.close();
    }
    config.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.cache~",
      .options = { { "rbind" } },
      .source = bundleDir / "ld.so.cache~",
      .type = "bind",
    });

    // dump to bundle
    auto bundleCfg = bundleDir / "config.json";
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

    auto bundleArg = "--bundle=" + bundleDir.string();

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
