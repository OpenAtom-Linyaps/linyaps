// SPDX-FileCopyrightText: 2024 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "light_elf.h"
#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "sha256.h"

#include <gelf.h>
#include <getopt.h>
#include <linux/limits.h>
#include <nlohmann/json.hpp>
#include <sys/mount.h>

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int erofsfuse_main(int argc, char **argv);

namespace {

std::atomic_bool mountFlag{ false };  // NOLINT
std::atomic_bool createFlag{ false }; // NOLINT
std::filesystem::path mountPoint;     // NOLINT
constexpr std::size_t default_page_size = 4096;

constexpr auto usage = u8R"(Linglong Universal Application Bundle

An offline distribution executable bundle of linglong.

Usage:
uabBundle [uabOptions...] [-- loaderOptions...]

Options:
    --extract=PATH extract the read-only filesystem image which is in the 'linglong.bundle' segment of uab to PATH. [exclusive]
    --print-meta print content of json which from the 'linglong.meta' segment of uab to STDOUT [exclusive]
    --help print usage of uab [exclusive]
)";

enum uabOption : std::uint8_t {
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

std::string resolveRealPath(const std::string &source) noexcept
{
    std::array<char, PATH_MAX + 1> resolvedPath{};

    auto *ptr = ::realpath(source.data(), resolvedPath.data());
    if (ptr == nullptr) {
        std::cerr << "failed to resolve path:" << ::strerror(errno) << std::endl;
        return {};
    }

    return { ptr };
}

std::size_t getChunkSize(std::size_t bundleSize) noexcept
{
    std::size_t page_size{ default_page_size };
    const auto ret = sysconf(_SC_PAGESIZE);
    if (ret > 0) {
        page_size = ret;
    }

    std::size_t block_size{ 0 };
    struct statvfs fs_info{};
    if (statvfs(".", &fs_info) > 0) {
        block_size = fs_info.f_bsize;
    }

    const auto base_block = std::max(page_size, block_size);
    if (bundleSize <= static_cast<std::size_t>(10 * 1024 * 1024)) {
        return 64 * base_block;
    }

    if (bundleSize <= static_cast<std::size_t>(100 * 1024 * 1024)) {
        return 128 * base_block;
    }

    return 256 * base_block;
}

std::string calculateDigest(int fd, std::size_t bundleOffset, std::size_t bundleLength) noexcept
{
    digest::SHA256 sha256;
    std::array<std::byte, 32> digest{};
    auto *mem = mmap(nullptr, bundleLength, PROT_READ, MAP_PRIVATE, fd, bundleOffset);
    if (mem != MAP_FAILED) {
        posix_madvise(mem, bundleLength, POSIX_FADV_WILLNEED | POSIX_FADV_SEQUENTIAL);
        sha256.update(reinterpret_cast<std::byte *>(mem), bundleLength);
        if (munmap(mem, bundleLength) == -1) {
            std::cerr << "munmap error:" << ::strerror(errno) << std::endl;
        }
    } else {
        // fallback to read blocks
        posix_fadvise(fd, bundleOffset, bundleLength, POSIX_FADV_WILLNEED | POSIX_FADV_SEQUENTIAL);
        std::align_val_t alignment{ default_page_size };
        if (auto ret = sysconf(_SC_PAGESIZE); ret > 0) {
            alignment = static_cast<std::align_val_t>(ret);
        }

        auto chunkSize = getChunkSize(bundleLength);
        auto *buf = ::operator new(chunkSize, alignment, std::nothrow);
        if (buf == nullptr) {
            std::cerr << "failed to allocate aligned memory" << std::endl;
            return {};
        }

        auto deleter = [alignment](void *ptr) noexcept {
            ::operator delete(ptr, alignment, std::nothrow);
        };
        std::unique_ptr<std::byte, decltype(deleter)> buffer{ reinterpret_cast<std::byte *>(buf),
                                                              deleter };

        std::size_t totalRead{ 0 };
        while (totalRead < bundleLength) {
            auto remaining = bundleLength - totalRead;
            auto readBytes = std::min(remaining, chunkSize);

            auto bytesRead = pread(fd, buffer.get(), readBytes, bundleOffset + totalRead);
            if (bytesRead < 0) {
                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "read uab error:" << ::strerror(errno) << std::endl;
                return {};
            }

            if (bytesRead == 0) {
                break;
            }

            sha256.update(buffer.get(), bytesRead);
            totalRead += bytesRead;
        }
    }

    sha256.final(digest.data());

    std::stringstream stream;
    stream << std::setfill('0') << std::hex;

    for (auto v : digest) {
        stream << std::setw(2) << static_cast<unsigned int>(v);
    }

    return stream.str();
}

std::optional<std::filesystem::path> find_fusermount() noexcept
{
    auto *pathEnv = getenv("PATH");
    if (pathEnv == nullptr) {
        return std::nullopt;
    }

    auto search_dir = [](const std::filesystem::path &dir) -> std::optional<std::filesystem::path> {
        std::error_code ec;
        auto iter = std::filesystem::directory_iterator{
            dir,
            std::filesystem::directory_options::skip_permission_denied,
            ec
        };

        if (ec) {
            std::cerr << "failed to open directory " << dir << ": " << ec.message() << std::endl;
            return std::nullopt;
        }

        for (const auto &entry : iter) {
            std::string filename = entry.path().filename();
            if (filename.rfind("fusermount", 0) != 0) {
                continue;
            }

            if (filename.substr(10).find_first_not_of("0123456789") != std::string::npos) {
                continue;
            }

            struct stat sb{};
            if (stat(entry.path().c_str(), &sb) == -1) {
                std::cerr << "stat error: " << strerror(errno) << std::endl;
                continue;
            }

            if (sb.st_uid != 0 || (sb.st_mode & S_ISUID) == 0) {
                std::cerr << "skip " << entry.path() << std::endl;
                continue;
            }

            return entry.path();
        }

        return std::nullopt;
    };

    std::stringstream ss{ pathEnv };
    std::string path;
    while (std::getline(ss, path, ':')) {
        auto res = search_dir(path);
        if (res) {
            return res;
        }
    }

    return std::nullopt;
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

        if (getenv("FUSERMOUNT_PROG") == nullptr) {
            auto fuserMountProg = find_fusermount();
            if (fuserMountProg) {
                setenv("FUSERMOUNT_PROG", fuserMountProg.value().c_str(), 1);
                std::cerr << "use fusermount:" << fuserMountProg->string() << std::endl;
            } else {
                std::cerr << "fusermount not found" << std::endl;
            }
        }

        return erofsfuse_main(4, const_cast<char **>(erofs_argv.data()));
    }

    int status{ 0 };
    while (true) {
        auto ret = ::waitpid(fusePid, &status, 0);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "waitpid() failed:" << ::strerror(errno) << std::endl;
            return -1;
        }
        break;
    }

    int ret{ -1 };
    if (WIFEXITED(status)) {
        auto code = WEXITSTATUS(status);
        ret = code;
    } else if (WIFSIGNALED(status)) {
        auto sig = WTERMSIG(status);
        std::cerr << "erofsfuse terminated due to signal " << strsignal(sig) << std::endl;
    }

    return ret;
}

void cleanResource() noexcept
{
    if (!mountFlag.load(std::memory_order_relaxed)) {
        return;
    }

    auto pid = fork();
    if (pid < 0) {
        std::cerr << "fork() error" << ": " << ::strerror(errno) << std::endl;
        return;
    }

    if (pid == 0) {
        if (::execlp("fusermount", "fusermount", "-z", "-u", mountPoint.c_str(), nullptr) == -1) {
            std::cerr << "fusermount error: " << ::strerror(errno) << std::endl;
            ::_exit(1);
        }

        ::_exit(0);
    }

    int status{ 0 };
    auto ret = ::waitpid(pid, &status, 0);
    if (ret == -1) {
        std::cerr << "wait failed:" << ::strerror(errno) << std::endl;
        return;
    }
    mountFlag.store(false, std::memory_order_relaxed);

    if (!createFlag.load(std::memory_order_relaxed)) {
        return;
    }

    // try to remove mount point
    std::error_code ec;
    if (std::filesystem::remove_all(mountPoint, ec) == static_cast<std::uintmax_t>(-1) && ec) {
        std::cerr << "failed to remove mount point:" << ec.message() << std::endl;
        return;
    }

    createFlag.store(false, std::memory_order_relaxed);
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
        // TODO: maybe not async safe, find a better way to handle signal
        cleanAndExit(128 + sig);
    };
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

    auto runtimeDir = resolveRealPath(runtimeDirPtr);
    if (runtimeDir.empty()) {
        return -1;
    }
    auto mountPointPath = std::filesystem::path{ runtimeDir } / "linglong" / "UAB" / uuid;

    std::error_code ec;
    if (!std::filesystem::create_directories(mountPointPath, ec) && ec) {
        std::cerr << "couldn't create mount point " << mountPointPath << ": " << ec.message()
                  << std::endl;
        return ec.value();
    }

    mountPoint = std::move(mountPointPath);
    createFlag.store(true, std::memory_order_relaxed);

    return 0;
}

std::optional<linglong::api::types::v1::UabMetaInfo>
getMetaInfo(const lightElf::native_elf &elf) noexcept
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

    std::optional<linglong::api::types::v1::UabMetaInfo> meta;
    try {
        auto json = nlohmann::json::parse(content);
        meta = json.get<linglong::api::types::v1::UabMetaInfo>();
    } catch (const nlohmann::json::parse_error &e) {
        std::cerr << "exception: " << e.what() << std::endl;
        return std::nullopt;
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

int runAppLoader(bool onlyApp, const std::vector<std::string_view> &loaderArgs) noexcept
{
    auto loader = mountPoint / "loader";
    std::error_code ec;
    if (!std::filesystem::exists(loader, ec)) {
        if (ec) {
            std::cerr << "failed to get loader status" << std::endl;
            return -1;
        }

        std::cout << "This UAB is not support for runnning" << std::endl;
        return 0;
    }

    auto argc = loaderArgs.size() + 2;
    auto *argv = new (std::nothrow) const char *[argc]();
    if (argv == nullptr) {
        std::cerr << "out of memory, exit." << std::endl;
        return ENOMEM;
    }

    auto deleter = defer([argv] {
        delete[] argv;
    });

    argv[0] = loader.c_str();
    argv[argc - 1] = nullptr;
    for (std::size_t i = 0; i < loaderArgs.size(); ++i) {
        argv[i + 1] = loaderArgs[i].data();
    }

    auto loaderPid = fork();
    if (loaderPid < 0) {
        std::cerr << "fork() error" << ": " << ::strerror(errno) << std::endl;
        return errno;
    }

    if (loaderPid == 0) {
        if (onlyApp && ::setenv("LINGLONG_UAB_LOADER_ONLY_APP", "true", 1) < 0) {
            std::cerr << "setenv error: " << ::strerror(errno) << std::endl;
            return errno;
        }

        if (::execv(loader.c_str(), reinterpret_cast<char *const *>(const_cast<char **>(argv)))
            == -1) {
            std::cerr << "execv(" << loader << ") error: " << ::strerror(errno) << std::endl;
            return errno;
        }
    }

    int status{ 0 };
    auto ret = ::waitpid(loaderPid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid failed:" << ::strerror(errno) << std::endl;
        return errno;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        // maybe we runnning under a shell
        return 128 + WTERMSIG(status);
    }

    std::cerr << "unknown exit state of loader" << std::endl;
    return -1;
}

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
        return ret;
    }

    mountFlag.store(true, std::memory_order_relaxed);
    return 0;
}
} // namespace

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

    // for cleaning up mount point
    if (std::atexit(cleanResource) != 0) {
        std::cerr << "failed register exit handler" << std::endl;
        return 1;
    }

    std::set_terminate([]() {
        cleanResource();
        std::abort();
    });

    if (!opts.extractPath.empty()) {
        if (auto ret = mountSelf(elf, metaInfo); ret != 0) {
            return ret;
        }

        return extractBundle(opts.extractPath);
    }

    if (auto ret = mountSelf(elf, metaInfo); ret != 0) {
        return ret;
    }

    bool onlyApp = metaInfo.onlyApp.value_or(false);
    if (onlyApp) {
        std::string appID;
        std::string module;
        for (const auto &layer : metaInfo.layers) {
            if (layer.info.kind == "app") {
                appID = layer.info.id;
                module = layer.info.packageInfoV2Module;
                break;
            }
        }

        if (appID.empty() || module.empty()) {
            std::cerr << "failed to find appID and module" << std::endl;
            return 1;
        }

        std::string envAppRoot =
          std::string(mountPoint) + "/layers/" + appID + "/" + module + "/files";
        if (::setenv("LINGLONG_UAB_APPROOT", const_cast<char *>(envAppRoot.data()), 1) == -1) {
            std::cerr << "setenv error: " << ::strerror(errno) << std::endl;
            return 1;
        }
    }

    return runAppLoader(onlyApp, opts.loaderArgs);
}
