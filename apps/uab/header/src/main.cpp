// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "light_elf.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "sha256.h"
#include "utils.h"

#include <gelf.h>
#include <getopt.h>
#include <linux/limits.h>
#include <nlohmann/json.hpp>
#include <sys/mount.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int erofsfuse_main(int argc, char **argv);

static std::atomic_bool mountFlag{ false };  // NOLINT
static std::atomic_bool createFlag{ false }; // NOLINT
static std::filesystem::path mountPoint;     // NOLINT

constexpr auto usage = u8R"(Linglong Universal Application Bundle

An offline distribution executable bundle of linglong.

Usage:
uabBundle [uabOptions...] [-- loaderOptions...]

Options:
    --extract=PATH extract the read-only filesystem image which is in the 'linglong.bundle' segment of uab to PATH. [exclusive]
    --print-meta print content of json which from the 'linglong.meta' segment of uab to STDOUT [exclusive]
    --help print usage of uab [exclusive]
)";

std::string resolveRealPath(std::string_view source) noexcept
{
    std::array<char, PATH_MAX + 1> resolvedPath{};

    auto *ptr = ::realpath(source.data(), resolvedPath.data());
    if (ptr == nullptr) {
        std::cerr << "failed to resolve path:" << ::strerror(errno) << std::endl;
        return {};
    }

    return { ptr };
}

std::string calculateDigest(int fd, std::size_t bundleOffset, std::size_t bundleLength) noexcept
{
    digest::SHA256 sha256;
    std::array<std::byte, 4096> buf{};
    std::array<std::byte, 32> md_value{};
    auto expectedRead = buf.size();
    int readLength{ 0 };

    while ((readLength = ::pread(fd, buf.data(), expectedRead, bundleOffset)) != 0) {
        if (readLength == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "read uab error:" << ::strerror(errno) << std::endl;
            return {};
        }

        sha256.update(buf.data(), readLength);

        bundleLength -= readLength;
        if (bundleLength == 0) {
            sha256.final(md_value.data());
            break;
        }

        bundleOffset += readLength;
        expectedRead = bundleLength > buf.size() ? buf.size() : bundleLength;
    }

    std::stringstream stream;
    stream << std::setfill('0') << std::hex;

    for (auto v : md_value) {
        stream << std::setw(2) << static_cast<unsigned int>(v);
    }

    return stream.str();
}

int mountSelfBundle(const lightElf::native_elf &elf,
                    const linglong::api::types::v1::UabMetaInfo &meta) noexcept
{
    auto bundleSh = elf.getSectionHeader(meta.sections.bundle);
    if (!bundleSh) {
        std::cerr << "couldn't get bundle section '" << meta.sections.bundle << "'" << std::endl;
        return -1;
    }

    auto bundleOffset = bundleSh->sh_offset;
    if (auto digest = calculateDigest(elf.underlyingFd(), bundleOffset, bundleSh->sh_size);
        digest != meta.digest) {
        std::cerr << "sha256 mismatched, expected: " << meta.digest << " calculated: " << digest
                  << std::endl;
        return -1;
    }

    auto selfBin = elf.absolutePath();
    auto offsetStr = "--offset=" + std::to_string(bundleOffset);
    std::array<const char *, 4> erofs_argv = { "erofsfuse",
                                               offsetStr.c_str(),
                                               selfBin.c_str(),
                                               mountPoint.c_str() };

    auto fusePid = fork();
    if (fusePid < 0) {
        std::cerr << "fork() error:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (fusePid == 0) {
        auto *maskOutput = ::getenv("UAB_EROFSFUSE_VERBOSE");
        if (maskOutput == nullptr) {
            auto tmpfd = ::open("/tmp", O_TMPFILE | O_WRONLY, S_IRUSR | S_IWUSR);
            if (tmpfd != -1) {
                ::dup2(tmpfd, STDOUT_FILENO);
                ::dup2(tmpfd, STDERR_FILENO);
            }
        }

        return erofsfuse_main(4, const_cast<char **>(erofs_argv.data()));
    }

    int status{ 0 };
    auto ret = ::waitpid(fusePid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    ret = WEXITSTATUS(status);
    if (ret != 0) {
        std::cerr << "couldn't mount bundle, fuse error code:" << ret << std::endl;
        ret = -1;
    }

    return ret;
}

void cleanResource() noexcept
{
    auto umountRet = [] {
        if (!mountFlag.load(std::memory_order_relaxed)) {
            return true;
        }

        auto pid = fork();
        if (pid < 0) {
            std::cerr << "fork() error" << ": " << ::strerror(errno) << std::endl;
            return false;
        }

        if (pid == 0) {
            if (::execlp("fusermount", "fusermount", "-z", "-u", mountPoint.c_str(), nullptr)
                == -1) {
                std::cerr << "fusermount error: " << ::strerror(errno) << std::endl;
                return false;
            }
        }

        int status{ 0 };
        auto ret = ::waitpid(pid, &status, 0);
        if (ret == -1) {
            std::cerr << "wait failed:" << ::strerror(errno) << std::endl;
            return false;
        }

        mountFlag.store(false, std::memory_order_relaxed);
        return true;
    }();

    if (umountRet && createFlag.load(std::memory_order_relaxed)) {
        std::error_code ec;
        if (!std::filesystem::exists(mountPoint, ec)) {
            if (ec) {
                std::cerr << "filesystem error " << mountPoint << ":" << ec.message() << std::endl;
                return;
            }

            createFlag.store(false, std::memory_order_relaxed);
            return;
        }

        if (std::filesystem::remove_all(mountPoint, ec) == static_cast<std::uintmax_t>(-1) || ec) {
            std::cerr << "failed to remove mount point:" << ec.message() << std::endl;
            return;
        }

        createFlag.store(false, std::memory_order_relaxed);
    }
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

    struct sigaction sa{};

    sa.sa_handler = handler;
    sa.sa_mask = blocking_mask;
    sa.sa_flags = 0;

    for (auto sig : quitSignals) {
        sigaction(sig, &sa, nullptr);
    }
}

int createMountPoint(std::string_view uuid) noexcept
{
    if (createFlag.load(std::memory_order_relaxed)) {
        std::cout << "mount point already has been created" << std::endl;
        return 0;
    }

    const char *runtimeDirPtr{ nullptr };
    runtimeDirPtr = ::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirPtr == nullptr) {
        std::cerr << "failed to get XDG_RUNTIME_DIR, fallback to /tmp" << std::endl;
        runtimeDirPtr = "/tmp";
    }

    auto mountPointPath =
      std::filesystem::path{ resolveRealPath(runtimeDirPtr) } / "linglong" / "UAB" / uuid;

    std::error_code ec;
    if (!std::filesystem::create_directories(mountPointPath, ec)) {
        if (ec.value() != EEXIST) {
            std::cerr << "couldn't create mount point " << mountPoint << ": " << ec.message()
                      << std::endl;
            return ec.value();
        }
    }

    mountPoint = std::move(mountPointPath);
    createFlag.store(true, std::memory_order_relaxed);

    return 0;
}

std::optional<linglong::api::types::v1::UabMetaInfo> getMetaInfo(const lightElf::native_elf &elf)
{
    auto metaSh = elf.getSectionHeader("linglong.meta");
    if (!metaSh) {
        std::cerr << "couldn't find meta section" << std::endl;
        return std::nullopt;
    }

    std::string content(metaSh->sh_size, '\0');
    if (::pread(elf.underlyingFd(), content.data(), metaSh->sh_size, metaSh->sh_offset) == -1) {
        std::cerr << "read failed:" << ::strerror(errno) << std::endl;
        return {};
    }

    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(content);
    } catch (const std::exception &ex) {
        std::cerr << "exception: " << ex.what() << std::endl;
    }

    return meta;
}

int extractBundle(std::string_view destination) noexcept
{
    std::error_code ec;
    if (!std::filesystem::create_directories(destination, ec) && ec) {
        std::cerr << "failed to create " << destination << ": " << ec.message() << std::endl;
        return ec.value();
    }

    auto options =
      std::filesystem::copy_options::copy_symlinks | std::filesystem::copy_options::recursive;
    std::filesystem::copy(mountPoint, destination, options, ec);
    if (ec) {
        std::cerr << "failed to extract bundle from " << mountPoint << " to " << destination << ": "
                  << ec.message() << std::endl;
        return ec.value();
    }

    return 0;
}

[[noreturn]] void runAppLoader(bool onlyApp,
                               const std::vector<std::string_view> &loaderArgs) noexcept
{
    auto loader = mountPoint / "loader";
    auto loaderStr = loader.string();
    auto argc = loaderArgs.size() + 2;
    auto *argv = new (std::nothrow) const char *[argc]();
    if (argv == nullptr) {
        std::cerr << "out of memory, exit." << std::endl;
        cleanAndExit(ENOMEM);
    }

    auto deleter = defer([argv] {
        delete[] argv;
    });

    argv[0] = loaderStr.c_str();
    argv[argc - 1] = nullptr;
    for (std::size_t i = 0; i < loaderArgs.size(); ++i) {
        argv[i + 1] = loaderArgs[i].data();
    }

    auto loaderPid = fork();
    if (loaderPid < 0) {
        std::cerr << "fork() error" << ": " << ::strerror(errno) << std::endl;
        cleanAndExit(errno);
    }

    if (loaderPid == 0) {
        std::string_view newEnv{ "LINGLONG_UAB_LOADER_ONLY_APP=true" };
        if (onlyApp && ::putenv(const_cast<char *>(newEnv.data())) < 0) {
            std::cerr << "putenv error: " << ::strerror(errno) << std::endl;
            cleanAndExit(errno);
        }

        if (::execv(loaderStr.c_str(), reinterpret_cast<char *const *>(const_cast<char **>(argv)))
            == -1) {
            std::cerr << "execv(" << loaderStr << ") error: " << ::strerror(errno) << std::endl;
            cleanAndExit(errno);
        }
    }

    int status{ 0 };
    auto ret = ::waitpid(loaderPid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid failed:" << ::strerror(errno) << std::endl;
        cleanAndExit(errno);
    }

    cleanAndExit(WEXITSTATUS(status));
}

enum uabOption {
    Help = 1,
    Extract,
    Meta,
};

struct argOption
{
    bool help{ false };
    bool printMeta{ false };
    std::string extractPath;
    std::vector<std::string_view> loaderArgs;
};

argOption parseArgs(const std::vector<std::string_view> &args)
{
    argOption opts;
    auto splitter = std::find_if(args.cbegin(), args.cend(), [](std::string_view arg) {
        return arg == "--";
    });

    auto uabArgc = args.size();
    if (splitter != args.cend()) {
        uabArgc = std::distance(args.cbegin(), splitter);
        opts.loaderArgs.assign(splitter + 1, args.cend());
    }

    std::array<struct option, 4> long_options{
        { { "print-meta", no_argument, nullptr, uabOption::Meta },
          { "extract", required_argument, nullptr, uabOption::Extract },
          { "help", no_argument, nullptr, uabOption::Help },
          { nullptr, 0, nullptr, 0 } }
    };

    std::vector<const char *> rawArgs;
    std::for_each(args.cbegin(), args.cend(), [&rawArgs](std::string_view arg) {
        rawArgs.emplace_back(arg.data());
    });

    int ch{ -1 };
    int counter{ 0 };
    while ((ch = ::getopt_long(uabArgc,
                               const_cast<char **>(rawArgs.data()),
                               "",
                               long_options.data(),
                               nullptr))
           != -1) {
        switch (ch) {
        case uabOption::Meta: {
            opts.printMeta = true;
            ++counter;
        } break;
        case uabOption::Extract: {
            opts.extractPath = optarg;
            ++counter;
        } break;
        case uabOption::Help: {
            opts.help = true;
            ++counter;
        } break;
        case '?':
            ::exit(EINVAL);
        default:
            throw std::logic_error("UNREACHABLE!! unknown option:" + std::to_string(ch));
        }
    }

    if (counter > 1) {
        std::cerr << "exclusive options has been detected" << std::endl;
        ::exit(-1);
    }

    return opts;
}

int mountSelf(const lightElf::native_elf &elf,
              const linglong::api::types::v1::UabMetaInfo &metaInfo) noexcept
{
    if (mountFlag.load(std::memory_order_relaxed)) {
        std::cout << "bundle already has been mounted" << std::endl;
        return 0;
    }

    const auto &uuid = metaInfo.uuid;
    if (auto ret = createMountPoint(uuid); ret != 0) {
        return ret;
    }

    if (auto ret = mountSelfBundle(elf, metaInfo); ret != 0) {
        return -1;
    }

    mountFlag.store(true, std::memory_order_relaxed);
    return 0;
}

int main(int argc, char **argv)
{
    handleSig();

    std::vector<std::string_view> args;
    args.reserve(argc);
    for (auto i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    auto selfBin = args.front();
    auto opts = parseArgs(args);

    if (opts.help) {
        std::cout << usage << std::endl;
        return 0;
    }

    lightElf::native_elf elf(selfBin);
    auto metaInfoRet = getMetaInfo(elf);
    if (!metaInfoRet) {
        std::cerr << "couldn't get metaInfo of this uab file" << std::endl;
        return -1;
    }
    const auto &metaInfo = *metaInfoRet;

    if (opts.printMeta) {
        std::cout << nlohmann::json(metaInfo).dump(4) << std::endl;
        return 0;
    }

    if (!opts.extractPath.empty()) {
        if (mountSelf(elf, metaInfo) != 0) {
            cleanAndExit(-1);
        }

        cleanAndExit(extractBundle(opts.extractPath));
    }

    if (mountSelf(elf, metaInfo) != 0) {
        cleanAndExit(-1);
    }

    bool onlyApp = metaInfo.onlyApp.value_or(false);
    if (onlyApp) {
        std::string appID, module;
        for (const auto &layer : metaInfo.layers) {
            if (layer.info.kind == "app") {
                appID = layer.info.id;
                module = layer.info.packageInfoV2Module;
                break;
            }
        }
        std::string envAppRoot =
          std::string(mountPoint) + "/layers/" + appID + "/" + module + "/files";
        if (-1 == ::setenv("LINGLONG_UAB_APPROOT", const_cast<char *>(envAppRoot.data()), 1)) {
            std::cerr << "setenv error: " << ::strerror(errno) << std::endl;
            cleanAndExit(errno);
        }
    }

    runAppLoader(onlyApp, opts.loaderArgs);
}
