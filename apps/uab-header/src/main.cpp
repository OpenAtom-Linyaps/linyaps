// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gelf.h>
#include <getopt.h>
#include <libelf.h>
#include <linux/limits.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sys/mount.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int erofsfuse_main(int argc, char **argv);

static std::atomic_bool mountFlag{ false };
static std::atomic_bool createFlag{ false };
static std::string mountPoint{};

constexpr auto usage = u8R"(Linglong Universal Application Bundle

An offline distribution executable bundle of linglong.

Usage:
uabBundle [uabOptions...] [-- loaderOptions...]

Options:
    --extract=PATH extract the read-only filesystem image which is in the 'linglong.bundle' segment of uab to PATH. [exclusive]
    --print-meta print content of json which from the 'linglong.meta' segment of uab to STDOUT [exclusive]
    --help print usage of uab [exclusive]
)";

std::string resolveRealPath(const std::string &source)
{
    std::string ret;
    std::array<char, PATH_MAX + 1> resolvedPath{};
    resolvedPath.fill(0);

    if (::realpath(source.c_str(), resolvedPath._M_elems) == nullptr) {
        std::cerr << "failed to resolve path:" << strerror(errno) << std::endl;
        return ret;
    }

    std::copy_n(resolvedPath.cbegin(), ::strlen(resolvedPath._M_elems), std::back_inserter(ret));
    return ret;
}

std::string detectLinglong() noexcept
{
    auto *pathPtr = ::getenv("PATH");
    if (pathPtr == nullptr) {
        std::cerr << "failed to get PATH" << std::endl;
        return {};
    }

    struct stat sb;
    std::string path{ pathPtr };
    std::string cliPath;
    size_t startPos = 0;
    size_t endPos = 0;

    while ((endPos = path.find(':', startPos)) != std::string::npos) {
        std::string binPath = path.substr(startPos, endPos - startPos) + "/ll-cli";
        if ((::stat(binPath.c_str(), &sb) == 0) && (sb.st_mode & S_IXOTH)) {
            cliPath = binPath;
            break;
        }

        startPos = endPos + 1;
    }

    return cliPath;
}

int importSelf(const std::string &cliBin, const char *uab) noexcept
{
    auto pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed:" << strerror(errno) << std::endl;
        return -1;
    }

    if (pid == 0) {
        return ::execl(cliBin.c_str(), cliBin.c_str(), "import", uab, nullptr);
    }

    int status{ 0 };
    auto ret = ::waitpid(pid, &status, 0);
    if (ret == -1) {
        std::cerr << "wait failed:" << strerror(errno) << std::endl;
        return -1;
    }

    if (auto result = WEXITSTATUS(status); result != 0) {
        std::cerr << "ll-cli import failed, return code:" << result << std::endl;
        ::exit(-1);
    }

    return 0;
}

std::optional<GElf_Shdr> getSectionHeader(int elfFd, const char *sectionName) noexcept
{
    std::error_code ec;

    auto elfPath = std::filesystem::read_symlink(std::filesystem::path{ "/proc/self/fd" }
                                                   / std::to_string(elfFd),
                                                 ec);
    if (ec) {
        std::cerr << "failed to get binary path:" << ec.message() << std::endl;
        return std::nullopt;
    }
    auto *elf = elf_begin(elfFd, ELF_C_READ, nullptr);
    if (elf == nullptr) {
        std::cerr << elfPath << " not usable:" << elf_errmsg(-1) << std::endl;
        return {};
    }

    size_t shdrstrndx{ 0 };
    if (elf_getshdrstrndx(elf, &shdrstrndx) == -1) {
        std::cerr << "failed to get section header index of bundle " << elfPath << ":"
                  << elf_errmsg(-1) << std::endl;
        return {};
    }

    Elf_Scn *scn = nullptr;
    std::optional<GElf_Shdr> metaSh;
    while ((scn = elf_nextscn(elf, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            std::cerr << "failed to get section header of bundle " << elfPath << ":"
                      << elf_errmsg(-1) << std::endl;
            break;
        }

        auto *sname = elf_strptr(elf, shdrstrndx, shdr.sh_name);
        if (::strcmp(sname, sectionName) == 0) {
            return shdr;
        }
    }

    return std::nullopt;
}

bool digestCheck(int fd,
                 std::size_t bundleOffset,
                 std::size_t bundleLength,
                 const std::string &expectedDigest) noexcept
{
    auto file = ::dup(fd);
    if (file == -1) {
        std::cerr << "dup() error:" << strerror(errno) << std::endl;
        return false;
    }

    if (::lseek(file, bundleOffset, SEEK_SET) == -1) {
        std::cerr << "lseek() error:" << strerror(errno) << std::endl;
        ::close(file);
        return false;
    }

    auto *ctx = EVP_MD_CTX_new();
    if (EVP_DigestInit_ex2(ctx, EVP_sha256(), nullptr) == 0) {
        std::cerr << "init digest context error" << std::endl;
        ::close(file);
        EVP_MD_CTX_free(ctx);
        return false;
    }

    std::array<unsigned char, 1024> buf{};
    std::array<unsigned char, EVP_MAX_MD_SIZE> md_value{};
    md_value.fill(0);
    buf.fill(0);

    auto expectedRead = bundleLength > buf.size() ? buf.size() : bundleLength;
    int readLength{ 0 };
    unsigned int digestLength{ 0 };
    while ((readLength = ::read(file, buf._M_elems, expectedRead)) != 0) {
        if (readLength == -1) {
            std::cerr << "read bundle error:" << strerror(errno) << std::endl;
            ::close(file);
            EVP_MD_CTX_free(ctx);
            return false;
        }

        if (readLength != expectedRead) {
            std::cout << "mismatched read during digest checking" << std::endl;
            ::close(file);
            EVP_MD_CTX_free(ctx);
            return false;
        }

        if (EVP_DigestUpdate(ctx, buf._M_elems, readLength) == 0) {
            std::cerr << "update digest error" << std::endl;
            ::close(file);
            EVP_MD_CTX_free(ctx);
            return false;
        }

        bundleLength -= readLength;

        if (bundleLength <= 0) {
            if (EVP_DigestFinal(ctx, md_value._M_elems, &digestLength) == 1) {
                break;
            }

            ::close(file);
            EVP_MD_CTX_free(ctx);
            std::cerr << "get digest error" << std::endl;
            return false;
        }

        expectedRead = bundleLength > buf.size() ? buf.size() : bundleLength;
    }

    std::stringstream stream;
    stream << std::setfill('0') << std::hex;

    for (auto i = 0U; i < digestLength; i++) {
        stream << std::setw(2) << static_cast<unsigned int>(md_value[i]);
    }

    auto digest = stream.str();
    bool same = (digest == expectedDigest);
    if (!same) {
        std::cerr << "sha256 mismatch, expected: " << expectedDigest << " calculated: " << digest
                  << std::endl;
    }

    ::close(file);
    EVP_MD_CTX_free(ctx);
    return same;
}

int mountSelfBundle(const char *selfBin, const nlohmann::json &meta) noexcept
{

    if (!meta.contains("sections") || !meta["sections"].contains("bundle")
        || !meta["sections"]["bundle"].is_string()) {
        std::cerr << "couldn't get the name of bundle section" << std::endl;
        return -1;
    }

    if (!meta.contains("digest") || !meta["digest"].is_string()) {
        std::cerr << "couldn't get the digest of bundle section" << std::endl;
        return -1;
    }

    auto selfBinFd = ::open(selfBin, O_RDONLY | O_CLOEXEC);
    if (selfBinFd == -1) {
        std::cerr << "failed to open bundle " << selfBin << std::endl;
        return {};
    }

    const std::string &sectionName = meta["sections"]["bundle"];
    auto bundleSh = getSectionHeader(selfBinFd, sectionName.c_str());
    if (!bundleSh) {
        ::close(selfBinFd);
        return -1;
    }

    auto bundleOffset = bundleSh->sh_offset;
    if (!digestCheck(selfBinFd, bundleOffset, bundleSh->sh_size, meta["digest"])) {
        ::close(selfBinFd);
        return -1;
    }

    auto offsetStr = "--offset=" + std::to_string(bundleOffset);
    const char *erofs_argv[] = { "erofsfuse", offsetStr.c_str(), selfBin, mountPoint.c_str() };

    auto fusePid = fork();
    if (fusePid < 0) {
        std::cerr << "fork() error:" << strerror(errno) << std::endl;
        ::close(selfBinFd);
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

        return erofsfuse_main(4, (char **)erofs_argv);
    }

    int status{ 0 };
    auto ret = ::waitpid(fusePid, &status, 0);
    if (ret == -1) {
        std::cerr << "wait failed:" << strerror(errno) << std::endl;
        ::close(selfBinFd);
        return -1;
    }

    ret = WEXITSTATUS(status);
    if (ret != 0) {
        std::cerr << "couldn't mount bundle, fuse error code:" << ret << std::endl;
        ret = -1;
    }

    ::close(selfBinFd);
    return ret;
}

void cleanResource() noexcept
{
    if (!createFlag.load()) {
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(mountPoint, ec)) {
        if (ec) {
            std::cerr << "filesystem error:" << ec.message() << std::endl;
        }
        return;
    }

    [] {
        if (!mountFlag.load()) {
            return;
        }

        auto pid = fork();
        if (pid < 0) {
            std::cerr << "fork() error" << ": " << strerror(errno) << std::endl;
            return;
        }

        if (pid == 0) {
            if (::execlp("umount", "umount", mountPoint.c_str()) == -1) {
                std::cerr << "umount error: " << strerror(errno) << std::endl;
                return;
            }
        }

        int status{ 0 };
        auto ret = ::waitpid(pid, &status, 0);
        if (ret == -1) {
            std::cerr << "wait failed:" << strerror(errno) << std::endl;
            return;
        }
    }();

    if (!std::filesystem::remove(mountPoint, ec)) {
        if (ec) {
            std::cerr << "failed to remove mount point:" << ec.message() << std::endl;
        }
    }
}

[[noreturn]] static void cleanAndExit(int exitCode) noexcept
{
    cleanResource();
    ::exit(exitCode);
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

int createMountPoint(const std::string &uuid) noexcept
{
    auto *runtimeDirPtr = ::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirPtr == nullptr) {
        std::cerr << "failed to get XDG_RUNTIME_DIR" << std::endl;
        return -1;
    }

    auto mountPointPath =
      std::filesystem::path{ resolveRealPath(runtimeDirPtr) } / "linglong/UAB" / uuid;

    std::error_code ec;
    if (!std::filesystem::create_directories(mountPointPath, ec)) {
        if (ec.value() != EEXIST) {
            std::cerr << "create mount point " << mountPoint << ": " << ec.message() << std::endl;
            return ec.value();
        }
    }

    mountPoint = mountPointPath.string();
    bool expected{ false };
    if (!createFlag.compare_exchange_strong(expected, true)) {
        std::cerr << "internal create flag error" << std::endl;
        return -1;
    }

    return 0;
}

nlohmann::json getMetaInfo(const char *uab) noexcept
{
    auto selfBinFd = ::open(uab, O_RDONLY);
    if (selfBinFd == -1) {
        std::cerr << "failed to open bundle " << uab << std::endl;
        return {};
    }

    auto metaSh = getSectionHeader(selfBinFd, "linglong.meta");
    if (!metaSh) {
        std::cerr << "couldn't find meta section" << std::endl;
        ::close(selfBinFd);
        return {};
    }

    if (::lseek(selfBinFd, metaSh->sh_offset, SEEK_SET) == -1) {
        std::cerr << "lseek failed:" << strerror(errno) << std::endl;
        ::close(selfBinFd);
        return {};
    }

    auto *buf = (char *)::malloc(sizeof(char) * metaSh->sh_size);
    if (buf == nullptr) {
        std::cerr << "malloc failed:" << strerror(errno) << std::endl;
        ::close(selfBinFd);
        return {};
    }

    if (::read(selfBinFd, buf, metaSh->sh_size) == -1) {
        std::cerr << "read failed:" << strerror(errno) << std::endl;
        ::close(selfBinFd);
        ::free(buf);
        return {};
    }

    std::string content(reinterpret_cast<char *>(buf));
    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(content);
    } catch (nlohmann::json::parse_error &ex) {
        std::cerr << "parse error: " << ex.what() << std::endl;
    }

    ::close(selfBinFd);
    ::free(buf);
    return meta;
}

int extractBundle(const std::string &destination) noexcept
{
    std::error_code ec;
    auto path = std::filesystem::path(destination);
    if (!std::filesystem::exists(path.parent_path(), ec)) {
        std::cerr << path.parent_path() << ": " << ec.message() << std::endl;
        return ec.value();
    }

    if (!std::filesystem::create_directory(path, path.parent_path(), ec)) {
        if (ec) {
            std::cerr << "create " << path << ":" << ec.message() << std::endl;
            return ec.value();
        }
    }

    if (!std::filesystem::is_directory(path, ec)) {
        std::cerr << path;
        if (ec) {
            std::cerr << ":" << ec.message();
        } else {
            std::cerr << " isn't a directory";
        }
        std::cerr << std::endl;
        return ec.value();
    }

    if (!std::filesystem::is_empty(path, ec)) {
        std::cerr << path;
        if (ec) {
            std::cerr << ":" << ec.message();
        } else {
            std::cerr << " isn't empty";
        }
        std::cerr << std::endl;
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

void runAppLoader(const std::vector<std::string> &loaderArgs) noexcept
{
    auto loader = std::filesystem::path{ mountPoint } / "loader";
    auto loaderStr = loader.string();
    auto argc = loaderArgs.size() + 2;
    auto *argv = (const char **)malloc(sizeof(char *) * argc);
    argv[0] = loaderStr.c_str();
    argv[argc - 1] = nullptr;
    for (std::size_t i = 0; i < loaderArgs.size(); ++i) {
        argv[i + 1] = loaderArgs[i].c_str();
    }

    auto loaderPid = fork();
    if (loaderPid < 0) {
        std::cerr << "fork() error" << ": " << strerror(errno) << std::endl;
        return;
    }

    if (loaderPid == 0) {
        if (::execv(loaderStr.c_str(), (char *const *)(argv)) == -1) {
            std::cerr << "execv(" << loaderStr << ") error: " << strerror(errno) << std::endl;
            return;
        }
    }

    int status{ 0 };
    auto ret = ::waitpid(loaderPid, &status, 0);
    if (ret == -1) {
        std::cerr << "wait failed:" << strerror(errno) << std::endl;
        return;
    }

    cleanAndExit(WEXITSTATUS(status));
}

[[noreturn]] void runAppLinglong(const std::string &cliBin,
                                 const nlohmann::json &info,
                                 const std::vector<std::string> &loaderArgs) noexcept
{
    if (!info.contains("id") || !info["id"].is_string()) {
        std::cerr << "couldn't get appId, stop to delegate runnning operation to linglong"
                  << std::endl;
        cleanAndExit(-1);
    }

    const auto &appId = info["id"];
    auto argc = loaderArgs.size() + 2;
    auto *argv = (const char **)malloc(sizeof(char *) * argc);
    argv[0] = cliBin.c_str();
    argv[argc - 1] = nullptr;

    for (std::size_t i = 0; i < loaderArgs.size(); ++i) {
        argv[i + 1] = loaderArgs[i].c_str();
    }

    cleanAndExit(::execv(cliBin.c_str(), (char *const *)(argv)));
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
    std::vector<std::string> loaderArgs;
};

argOption parseArgs(int argc, char **argv)
{
    argOption opts;
    int splitIndex{ -1 };
    for (std::size_t i = 0; i < argc; ++i) {
        if (::strcmp(argv[i], "--") == 0) {
            splitIndex = i;
            break;
        }
    }

    if (splitIndex != -1) {
        auto *loaderArgv = argv + splitIndex + 1;
        auto loaderArgc = argc - splitIndex - 1;
        std::vector<std::string> loaderArgs{ static_cast<std::size_t>(loaderArgc) };
        for (std::size_t i = 0; i < loaderArgc; ++i) {
            loaderArgs.emplace_back(loaderArgv[i]);
        }
        opts.loaderArgs = std::move(loaderArgs);
    }

    struct option long_options[] = { { "print-meta", no_argument, nullptr, uabOption::Meta },
                                     { "extract", required_argument, nullptr, uabOption::Extract },
                                     { "help", no_argument, nullptr, uabOption::Help },
                                     { nullptr, 0, nullptr, 0 } };
    int ch;
    int counter{ 0 };
    auto *uabArgv = argv;
    auto uabArgc = splitIndex == -1 ? argc : splitIndex;

    while ((ch = getopt_long(uabArgc, uabArgv, "", long_options, nullptr)) != -1) {
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
        default:
            break;
        }
    }

    if (counter > 1) {
        std::cerr << "exclusive options has been detected" << std::endl;
        ::exit(-1);
    }

    return opts;
}

int mountSelf(const char *selfBin, const nlohmann::json &metaInfo) noexcept
{
    if (!metaInfo.contains("uuid") || !metaInfo["uuid"].is_string()) {
        std::cerr << "couldn't get UUID from metaInfo" << std::endl;
        return -1;
    }

    const auto &uuid = metaInfo["uuid"];
    if (auto ret = createMountPoint(uuid); ret != 0) {
        return ret;
    }

    if (auto ret = mountSelfBundle(selfBin, metaInfo); ret != 0) {
        return -1;
    }

    bool expected{ false };
    if (!mountFlag.compare_exchange_strong(expected, true)) {
        std::cerr << "internal create flag error" << std::endl;
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    elf_version(EV_CURRENT);
    handleSig();

    auto *selfBin = argv[0];
    auto opts = parseArgs(argc, argv);

    if (opts.help) {
        std::cout << usage << std::endl;
        return 0;
    }

    auto metaInfo = getMetaInfo(selfBin);
    if (metaInfo.empty()) {
        return -1;
    }

    if (opts.printMeta) {
        std::cout << metaInfo.dump(4) << std::endl;
        return 0;
    }

    if (!opts.extractPath.empty()) {
        opts.extractPath = resolveRealPath(opts.extractPath);
        if (opts.extractPath.empty()) {
            std::cerr << "couldn't resolve extractPath" << std::endl;
            return -1;
        }

        if (mountSelf(selfBin, metaInfo) != 0) {
            cleanAndExit(-1);
        }

        cleanAndExit(extractBundle(opts.extractPath));
    }

    auto cliPath = detectLinglong();
    if (!cliPath.empty() && importSelf(cliPath, selfBin) == 0) {
        std::cout << "import uab to linglong successfully, delegate running operation to linglong"
                  << std::endl;

        if (!metaInfo.contains("layers") || !metaInfo["layers"].is_array()) {
            std::cerr << "couldn't get layers info from metaInfo" << std::endl;
            return -1;
        }

        const auto &appLayer = metaInfo["layers"].front();
        if (!appLayer.is_object() || !appLayer.contains("info") || !appLayer["info"].is_object()) {
            std::cerr << "invalid format of app layer" << std::endl;
            return -1;
        }

        auto info = appLayer["info"];
        runAppLinglong(cliPath, info, opts.loaderArgs);
    }

    if (mountSelf(selfBin, metaInfo) != 0) {
        cleanAndExit(-1);
    }

    runAppLoader(opts.loaderArgs);
}
