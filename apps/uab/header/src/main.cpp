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
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int erofsfuse_main(int argc, char **argv);

static std::atomic_bool mountFlag{ false };
static std::atomic_bool createFlag{ false };
static std::filesystem::path mountPoint;

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

std::string detectLinglong() noexcept
{
    auto *pathPtr = ::getenv("PATH");
    if (pathPtr == nullptr) {
        std::cerr << "failed to get PATH" << std::endl;
        return {};
    }

    struct stat sb
    {
    };

    std::string path{ pathPtr };
    std::string cliPath;
    size_t startPos = 0;
    size_t endPos = 0;

    while ((endPos = path.find(':', startPos)) != std::string::npos) {
        std::string binPath = path.substr(startPos, endPos - startPos) + "/ll-cli";
        if ((::stat(binPath.c_str(), &sb) == 0) && ((sb.st_mode & S_IXOTH) != 0U)) {
            cliPath = binPath;
            break;
        }

        startPos = endPos + 1;
    }

    return cliPath;
}

int importSelf(const std::string &cliBin, std::string_view appRef, const std::string &uab) noexcept
{
    std::array<int, 2> out{};
    if (pipe(out.data()) == -1) {
        std::cerr << "pipe() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    auto pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (pid == 0) {
        ::close(out[0]);
        if (::dup2(out[1], STDOUT_FILENO) == -1) {
            std::cerr << "dup2() failed in sub-process:" << ::strerror(errno) << std::endl;
            return -1;
        }

        if (::execl(cliBin.data(), cliBin.data(), "--json", "list", nullptr) == -1) {
            std::cerr << "execl() failed:" << ::strerror(errno) << std::endl;
            return -1;
        }
    }

    ::close(out[1]);
    auto closeReadPipe = defer([fd = out[0]] {
        ::close(fd);
    });

    std::array<char, PIPE_BUF> buf{};
    std::string content;
    auto bytesRead{ -1 };
    while ((bytesRead = ::read(out[0], buf.data(), buf.size())) != 0) {
        if (bytesRead == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "read failed:" << ::strerror(errno) << std::endl;
            return -1;
        }

        content.append(buf.data(), bytesRead);
    }

    int status{ 0 };
    auto ret = ::waitpid(pid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (auto result = WEXITSTATUS(status); result != 0) {
        std::cerr << "ll-cli --json list failed, return code:" << result << std::endl;
        return -1;
    }

    std::vector<linglong::api::types::v1::PackageInfoV2> packages;
    try {
        auto packagesJson = nlohmann::json::parse(content);
        packages = packagesJson.get<decltype(packages)>();
    } catch (nlohmann::detail::parse_error &e) {
        std::cerr << "parse content from ll-cli list output error:" << e.what() << std::endl;
        return -1;
    } catch (std::exception &e) {
        std::cerr << "catching an exception when parsing output of ll-cli list:" << e.what()
                  << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "catching unknown value" << std::endl;
        return -1;
    }

    for (const auto &package : packages) {
        auto curRef =
          package.channel + ":" + package.id + "/" + package.version + "/" + package.arch[0];
        if (curRef == appRef) {
            return 0; // already exist
        }
    }

    // install a new application
    pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (pid == 0) {
        if (::execl(cliBin.data(), cliBin.data(), "install", uab.data(), nullptr) == -1) {
            std::cerr << "execl() failed:" << ::strerror(errno) << std::endl;
            return -1;
        }
    }

    status = -1;
    ret = ::waitpid(pid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid() failed:" << ::strerror(errno) << std::endl;
        return -1;
    }

    if (auto result = WEXITSTATUS(status); result != 0) {
        std::cerr << "ll-cli install failed, return code:" << result << std::endl;
        return -1;
    }

    return 0;
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
            std::cerr << "fork() error" << ": " << strerror(errno) << std::endl;
            return false;
        }

        if (pid == 0) {
            if (::execlp("umount", "umount", "-l", mountPoint.c_str(), nullptr) == -1) {
                std::cerr << "umount error: " << strerror(errno) << std::endl;
                return false;
            }
        }

        int status{ 0 };
        auto ret = ::waitpid(pid, &status, 0);
        if (ret == -1) {
            std::cerr << "wait failed:" << strerror(errno) << std::endl;
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

        if (!std::filesystem::remove_all(mountPoint, ec) || ec) {
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
      std::filesystem::path{ resolveRealPath(runtimeDirPtr) } / "linglong/UAB" / uuid;

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

[[noreturn]] void runAppLoader(const linglong::api::types::v1::UabMetaInfo &meta,
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

[[noreturn]] void
runAppLinglong(std::string_view cliBin,
               const linglong::api::types::v1::UabLayer &layer,
               [[maybe_unused]] const std::vector<std::string_view> &loaderArgs) noexcept
{
    const auto &appId = layer.info.id;
    std::array<const char *, 4> argv{};
    argv[0] = cliBin.data();
    argv[1] = "run";
    argv[2] = appId.c_str();
    argv[3] = nullptr;

    cleanAndExit(
      ::execv(cliBin.data(), reinterpret_cast<char *const *>(const_cast<char **>(argv.data()))));
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

    const auto &layersRef = metaInfo.layers;
    const auto &appLayer = std::find_if(layersRef.cbegin(),
                                        layersRef.cend(),
                                        [](const linglong::api::types::v1::UabLayer &layer) {
                                            return layer.info.kind == "app";
                                        });

    if (appLayer == layersRef.cend()) {
        std::cerr << "couldn't find application layer" << std::endl;
        return -1;
    }
    const auto &appInfo = appLayer->info;

    auto cliPath = detectLinglong();
    auto appRef =
      appInfo.channel + ":" + appInfo.id + "/" + appInfo.version + "/" + appInfo.arch[0];
    if (!cliPath.empty()) {
        std::string uab{ selfBin.cbegin(), selfBin.cend() };
        if (importSelf(cliPath, appRef, uab) != 0) {
            std::cerr << "failed to import uab by ll-cli" << std::endl;
            return -1;
        }

        std::cout << "import uab to linglong successfully, delegate running operation to linglong."
                  << std::endl;

        runAppLinglong(cliPath, *appLayer, opts.loaderArgs);
    }

    if (mountSelf(elf, metaInfo) != 0) {
        cleanAndExit(-1);
    }

    runAppLoader(metaInfo, opts.loaderArgs);
}
