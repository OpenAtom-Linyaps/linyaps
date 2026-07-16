/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "configure.h"
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/LinglongAPIV1.hpp"
#include "linglong/api/types/v1/PackageInfoDisplay.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1PackageTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1PruneResult.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/cli/printer.h"
#include "linglong/common/dir.h"
#include "linglong/common/error.h"
#include "linglong/common/strings.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/package/layer_file.h"
#include "linglong/package/reference.h"
#include "linglong/package/version.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/namespace.h"
#include "linglong/utils/runtime_config.h"
#include "linglong/utils/xdp.h"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <fmt/ranges.h>
#include <linux/un.h>
#include <nlohmann/json.hpp>
#include <uuid.h>

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace linglong::utils::error;

namespace {

constexpr std::size_t ContainerIDDisplayLength = 12;
constexpr const char *DebugDevelopModule = "develop";
const std::filesystem::path BaseDebugFileDirectory{ "/usr/lib/debug" };

std::string makeDebugInstanceID()
{
    uuid_t uuid;
    uuid_generate_random(uuid);

    std::array<char, 37> uuidString{};
    uuid_unparse_lower(uuid, uuidString.data());
    return "debug-" + std::string{ uuidString.data() };
}

std::string gdbRemoteTarget(const std::string &listen) noexcept
{
    if (!listen.empty() && listen.front() == ':') {
        return "localhost" + listen;
    }

    return listen;
}

std::string shellQuote(const std::string &value)
{
    std::string quoted{ "'" };
    for (auto ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
            continue;
        }
        quoted += ch;
    }
    quoted += "'";

    return quoted;
}

std::vector<std::string> makeDebugCommand(const linglong::cli::RunOptions &options,
                                          std::vector<std::string> commands)
{
    commands.insert(commands.begin(), options.debugListen);
    commands.insert(commands.begin(), "--once");
    commands.insert(commands.begin(), "gdbserver");
    return commands;
}

std::string makeDebugSymbolDir(const linglong::runtime::RunContext &runContext)
{
    std::vector<std::filesystem::path> dirs{
        BaseDebugFileDirectory,
    };

    if (runContext.getRuntimeLayer()) {
        dirs.emplace_back(linglong::generator::ContainerCfgBuilder::runtimeMountPoint
                          / "lib/debug");
    }

    if (runContext.getAppLayer()) {
        dirs.emplace_back(
          linglong::generator::ContainerCfgBuilder::appMountPoint(runContext.getTargetID())
          / "lib/debug");
    }

    for (const auto &extension : runContext.getExtensionLayers()) {
        dirs.emplace_back(
          linglong::generator::ContainerCfgBuilder::extensionMountPoint(extension.getReference().id)
          / "lib/debug");
    }

    std::ostringstream stream;
    for (const auto &dir : dirs) {
        if (stream.tellp() > 0) {
            stream << ':';
        }
        stream << dir.string();
    }

    return stream.str();
}

std::string mergeDebugSymbolDir(const std::optional<std::string> &debugSymbolDir,
                                const std::string &defaultDebugSymbolDir)
{
    if (!debugSymbolDir || debugSymbolDir->empty()) {
        return defaultDebugSymbolDir;
    }

    if (defaultDebugSymbolDir.empty()) {
        return *debugSymbolDir;
    }

    return *debugSymbolDir + ":" + defaultDebugSymbolDir;
}

std::string makeDebugAttachScriptContent(const linglong::cli::RunOptions &options)
{
    std::ostringstream script;
    script << "#!/bin/sh\n";

    script << "set -- -ex " << shellQuote("target remote " + gdbRemoteTarget(options.debugListen))
           << " \"$@\"\n";
    if (options.debugSymbolDir && !options.debugSymbolDir->empty()) {
        script << "set -- -ex " << shellQuote("set debug-file-directory " + *options.debugSymbolDir)
               << " \"$@\"\n";
    }
    if (options.debugDebuginfod && !options.debugDebuginfod->empty()) {
        script << "DEBUGINFOD_URLS=" << shellQuote(*options.debugDebuginfod) << "\n";
        script << "export DEBUGINFOD_URLS\n";
        script << "if gdb -nx -batch -ex " << shellQuote("show debuginfod enabled")
               << " 2>&1 | grep -q " << shellQuote("^Debuginfod ") << "; then\n";
        script << "    set -- -ex " << shellQuote("set debuginfod urls " + *options.debugDebuginfod)
               << " \"$@\"\n";
        script << "    set -- -ex " << shellQuote("set debuginfod enabled on") << " \"$@\"\n";
        script << "fi\n";
    }
    script << "exec gdb \"$@\"\n";

    return script.str();
}

Result<std::filesystem::path> createDebugAttachScript(const linglong::cli::RunOptions &options)
{
    LINGLONG_TRACE("create debug attach script");

    std::error_code ec;
    auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return LINGLONG_ERR("failed to get temporary directory", ec);
    }

    uuid_t uuid;
    uuid_generate_random(uuid);
    std::array<char, 37> uuidString{};
    uuid_unparse_lower(uuid, uuidString.data());

    auto scriptPath = tempDir / ("linglong-gdb-" + std::string{ uuidString.data() } + ".sh");
    std::ofstream script{ scriptPath };
    if (!script) {
        return LINGLONG_ERR(fmt::format("failed to create {}", scriptPath.string()));
    }

    script << makeDebugAttachScriptContent(options);
    script.close();
    if (!script) {
        return LINGLONG_ERR(fmt::format("failed to write {}", scriptPath.string()));
    }

    std::filesystem::permissions(scriptPath,
                                 std::filesystem::perms::owner_read
                                   | std::filesystem::perms::owner_write
                                   | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace,
                                 ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to set {} executable", scriptPath.string()), ec);
    }

    return scriptPath;
}

void printDebugAttachHint(const linglong::cli::RunOptions &options)
{
    if (::isatty(::fileno(stdout)) == 0) {
        return;
    }

    auto script = createDebugAttachScript(options);
    if (!script) {
        std::cout << _("Debug mode is enabled. Attach from another terminal with gdb:")
                  << std::endl;
        if (options.debugDebuginfod && !options.debugDebuginfod->empty()) {
            std::cout << "  (shell) export DEBUGINFOD_URLS=" << shellQuote(*options.debugDebuginfod)
                      << std::endl;
            std::cout << "  (shell) gdb" << std::endl;
            std::cout << "  (gdb) # For newer gdb, you may also enable debuginfod explicitly."
                      << std::endl;
        }
        if (options.debugSymbolDir && !options.debugSymbolDir->empty()) {
            std::cout << "  (gdb) set debug-file-directory " << *options.debugSymbolDir
                      << std::endl;
        }
        std::cout << "  (gdb) target remote " << gdbRemoteTarget(options.debugListen) << std::endl;
        LogW("failed to create gdb attach script: {}", script.error().message());
        return;
    }

    auto scriptContent = makeDebugAttachScriptContent(options);
    std::cout << "============================================================" << std::endl;
    std::cout << _("Debug mode is enabled. Attach from another terminal with:") << std::endl;
    std::cout << "  " << script->string() << std::endl;
    std::cout << std::endl;
    std::cout << _("Generated gdb attach script:") << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << scriptContent;
    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "============================================================" << std::endl;
}

Result<std::filesystem::path> preparePeerSocketDir() noexcept
{
    LINGLONG_TRACE("prepare peer socket directory");

    auto peerSocketDirPattern = std::string{ "/tmp/linglong-package-manager-XXXXXX" };
    auto *path = ::mkdtemp(peerSocketDirPattern.data());
    if (path == nullptr) {
        return LINGLONG_ERR("failed to create peer socket directory", errno);
    }

    auto peerSocketDir = std::filesystem::path{ path };
    auto removePeerSocketDir = [&peerSocketDir] {
        std::error_code ec;
        std::filesystem::remove_all(peerSocketDir, ec);
        if (ec) {
            LogW("failed to remove peer socket directory {}: {}",
                 peerSocketDir.string(),
                 ec.message());
        }
    };

    auto *pw = ::getpwnam(LINGLONG_USERNAME);
    if (pw == nullptr) {
        removePeerSocketDir();
        return LINGLONG_ERR(fmt::format("failed to get user info for {}", LINGLONG_USERNAME));
    }

    if (::chown(peerSocketDir.c_str(), pw->pw_uid, pw->pw_gid) != 0) {
        removePeerSocketDir();
        return LINGLONG_ERR("failed to change peer socket directory owner", errno);
    }

    return peerSocketDir;
}

Result<void> waitForDBusPeerReady(const QString &service,
                                  const QString &path,
                                  const QDBusConnection &connection) noexcept
{
    LINGLONG_TRACE("wait for dbus peer ready");

    auto peer = linglong::api::dbus::v1::DBusPeer(service, path, connection);
    auto reply = peer.Ping();
    reply.waitForFinished();
    if (!reply.isValid()) {
        return LINGLONG_ERR(reply.error().message().toStdString());
    }

    return LINGLONG_OK;
}

std::vector<std::string> getAutoModuleList() noexcept
{
    auto getModuleFromLanguageEnv = [](const std::string &lang) -> std::vector<std::string> {
        if (lang.length() < 2) {
            return {};
        }

        if (!std::all_of(lang.begin(), lang.begin() + 2, [](char c) {
                return 'a' <= c && c <= 'z';
            })) {
            return {};
        }

        std::vector<std::string> modules;
        modules.push_back("lang_" + lang.substr(0, 2));

        if (lang.length() == 2) {
            return modules;
        }

        if (lang[2] == '.') {
            return modules;
        }

        if (lang[2] == '@') {
            return modules;
        }

        if (lang[2] != '_') {
            return {};
        }

        if (lang.length() < 5) {
            return {};
        }

        modules.push_back("lang_" + lang.substr(0, 5));

        if (lang.length() == 5) {
            return modules;
        }

        if (lang[5] == '.') {
            return modules;
        }

        if (lang[5] == '@') {
            return modules;
        }

        return {};
    };

    auto envs = {
        "LANG",           "LC_ADDRESS",  "LC_ALL",       "LC_IDENTIFICATION",
        "LC_MEASUREMENT", "LC_MESSAGES", "LC_MONETARY",  "LC_NAME",
        "LC_NUMERIC",     "LC_PAPER",    "LC_TELEPHONE", "LC_TIME",
    };

    std::vector<std::string> result = { "binary" };

    for (const auto &env : envs) {
        auto lang = getenv(env);
        if (lang == nullptr) {
            continue;
        }
        auto modules = getModuleFromLanguageEnv(lang);
        result.insert(result.end(), modules.begin(), modules.end());
    }

    std::sort(result.begin(), result.end());
    return { result.begin(), std::unique(result.begin(), result.end()) };
}

bool delegateToContainerInit(const std::string &containerID,
                             std::vector<std::string> commands) noexcept
{
    auto containerSocket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (containerSocket == -1) {
        return false;
    }

    auto cleanup = linglong::utils::finally::finally([containerSocket] {
        ::close(containerSocket);
    });

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    auto bundleDir = linglong::common::dir::getBundleDir(containerID);
    const std::string socketPath = bundleDir / "init/socket";

    std::copy(socketPath.begin(), socketPath.end(), &addr.sun_path[0]);
    addr.sun_path[socketPath.size() + 1] = 0;

    auto ret = ::connect(containerSocket,
                         reinterpret_cast<struct sockaddr *>(&addr),
                         offsetof(sockaddr_un, sun_path) + socketPath.size());
    if (ret == -1) {
        return false;
    }

    std::string bashContent;
    for (const auto &command : commands) {
        bashContent.append(linglong::common::strings::quoteBashArg(command));
        bashContent.push_back(' ');
    }

    commands.clear();
    commands.push_back(bashContent);
    commands.insert(commands.begin(), "-c");
    commands.insert(commands.begin(), "--login");
    commands.insert(commands.begin(), "bash");

    std::string command_data;
    for (const auto &command : commands) {
        command_data.append(command);
        command_data.push_back('\0');
    }
    command_data.push_back('\0');

    std::uint64_t rest_len = command_data.size();
    ret = ::send(containerSocket, &rest_len, sizeof(rest_len), 0);
    if (ret == -1) {
        return false;
    }

    while (rest_len > 0) {
        auto send_len = ::send(containerSocket, command_data.c_str(), rest_len, 0);
        if (send_len == -1) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        rest_len -= send_len;
    }

    if (rest_len != 0) {
        return false;
    }

    int result{ -1 };
    ret = ::recv(containerSocket, &result, sizeof(result), 0);
    LogD("delegate result: {}", result);
    return result == 0;
}

struct ModuleSize
{
    std::uint64_t exclusiveSize{ 0 };
    std::uint64_t sharedSize{ 0 };
    std::uint64_t logicalSize{ 0 };
    std::uint64_t actualSize{ 0 };
};

struct InodeKey
{
    dev_t device{ 0 };
    ino_t inode{ 0 };

    bool operator==(const InodeKey &that) const noexcept
    {
        return this->device == that.device && this->inode == that.inode;
    }
};

struct InodeKeyHash
{
    std::size_t operator()(const InodeKey &key) const noexcept
    {
        const auto deviceHash = std::hash<dev_t>{}(key.device);
        const auto inodeHash = std::hash<ino_t>{}(key.inode);
        return deviceHash ^ (inodeHash + 0x9e3779b9 + (deviceHash << 6) + (deviceHash >> 2));
    }
};

struct InodeUsage
{
    std::uint64_t diskUsage{ 0 };
    std::unordered_set<std::size_t> modules;
};

struct ModuleSizeCalculation
{
    std::vector<ModuleSize> moduleSizes;
    std::uint64_t actualTotalSize{ 0 };
};

bool versionLess(const std::string &lhs, const std::string &rhs) noexcept
{
    auto lhsVersion = linglong::package::Version::parse(lhs);
    auto rhsVersion = linglong::package::Version::parse(rhs);
    if (lhsVersion && rhsVersion) {
        if (*lhsVersion != *rhsVersion) {
            return *lhsVersion < *rhsVersion;
        }
    }

    return lhs < rhs;
}

bool moduleNameLess(const linglong::cli::Printer::ModuleSizeInfo &lhs,
                    const linglong::cli::Printer::ModuleSizeInfo &rhs) noexcept
{
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    if (lhs.channel != rhs.channel) {
        return lhs.channel < rhs.channel;
    }
    if (lhs.module != rhs.module) {
        return lhs.module < rhs.module;
    }

    return versionLess(lhs.version, rhs.version);
}

Result<ModuleSizeCalculation>
calculateModuleSizes(const std::vector<std::filesystem::path> &moduleDirs) noexcept
{
    LINGLONG_TRACE("calculate module sizes");

    ModuleSizeCalculation calculation;
    calculation.moduleSizes.resize(moduleDirs.size());
    std::unordered_map<InodeKey, InodeUsage, InodeKeyHash> inodeUsages;
    std::error_code ec;

    auto addEntry = [&](const std::filesystem::path &path,
                        std::size_t moduleIndex) -> Result<void> {
        struct stat64 st{};
        if (::lstat64(path.c_str(), &st) == -1) {
            const auto err = errno;
            return LINGLONG_ERR(fmt::format("failed to stat {}: {}",
                                            path,
                                            linglong::common::error::errorString(err)));
        }

        const auto diskUsage = static_cast<std::uint64_t>(st.st_blocks) * 512;
        if (st.st_nlink == 1) {
            auto &moduleSize = calculation.moduleSizes[moduleIndex];
            moduleSize.exclusiveSize += diskUsage;
            moduleSize.logicalSize += diskUsage;
            moduleSize.actualSize += diskUsage;
            calculation.actualTotalSize += diskUsage;
            return LINGLONG_OK;
        }

        const auto inodeKey = InodeKey{ st.st_dev, st.st_ino };
        auto &usage = inodeUsages[inodeKey];
        usage.diskUsage = diskUsage;
        usage.modules.insert(moduleIndex);

        return LINGLONG_OK;
    };

    for (std::size_t moduleIndex = 0; moduleIndex < moduleDirs.size(); ++moduleIndex) {
        const auto &dir = moduleDirs[moduleIndex];
        auto addRoot = addEntry(dir, moduleIndex);
        if (!addRoot) {
            return LINGLONG_ERR(addRoot);
        }

        auto iterator = std::filesystem::recursive_directory_iterator{
            dir,
            std::filesystem::directory_options::skip_permission_denied,
            ec
        };
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to open module directory {}", dir), ec);
        }

        const auto end = std::filesystem::recursive_directory_iterator{};
        while (iterator != end) {
            const auto &entry = *iterator;
            auto addResult = addEntry(entry.path(), moduleIndex);
            if (!addResult) {
                return LINGLONG_ERR(addResult);
            }

            iterator.increment(ec);
            if (ec) {
                return LINGLONG_ERR(fmt::format("failed to iterate module directory {}", dir), ec);
            }
        }
    }

    for (const auto &[_, usage] : inodeUsages) {
        const auto moduleCount = usage.modules.size();
        if (moduleCount == 0) {
            continue;
        }

        calculation.actualTotalSize += usage.diskUsage;
        const auto actualSize = usage.diskUsage / moduleCount;
        for (auto moduleIndex : usage.modules) {
            auto &moduleSize = calculation.moduleSizes[moduleIndex];
            moduleSize.logicalSize += usage.diskUsage;
            if (moduleCount == 1) {
                moduleSize.exclusiveSize += usage.diskUsage;
                moduleSize.actualSize += usage.diskUsage;
            } else {
                moduleSize.sharedSize += usage.diskUsage;
                moduleSize.actualSize += actualSize;
            }
        }
    }

    return calculation;
}

Result<std::uint64_t> calculateRealDiskUsage(const std::filesystem::path &dir) noexcept
{
    LINGLONG_TRACE("calculate real disk usage");

    std::uint64_t size{ 0 };
    std::unordered_set<InodeKey, InodeKeyHash> visitedInodes;

    auto addPath = [&](const std::filesystem::path &path) -> Result<void> {
        struct stat64 st{};

        if (::lstat64(path.c_str(), &st) == -1) {
            const auto err = errno;
            return LINGLONG_ERR(fmt::format("failed to stat {}: {}",
                                            path,
                                            linglong::common::error::errorString(err)));
        }

        if (st.st_nlink > 1) {
            const auto inodeKey = InodeKey{ st.st_dev, st.st_ino };
            if (!visitedInodes.insert(inodeKey).second) {
                return LINGLONG_OK;
            }
        }

        size += static_cast<std::uint64_t>(st.st_blocks) * 512;
        return LINGLONG_OK;
    };

    auto rootResult = addPath(dir);
    if (!rootResult) {
        return LINGLONG_ERR(rootResult);
    }

    std::error_code ec;
    auto iterator = std::filesystem::recursive_directory_iterator{
        dir,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    };
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to open repository directory {}", dir), ec);
    }

    const auto end = std::filesystem::recursive_directory_iterator{};
    while (iterator != end) {
        const auto &entry = *iterator;
        auto result = addPath(entry.path());
        if (!result) {
            return LINGLONG_ERR(result);
        }

        iterator.increment(ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to iterate repository directory {}", dir), ec);
        }
    }

    return size;
}

using DependsNode = linglong::cli::Printer::DependsNode;

DependsNode &appendDependsNode(std::vector<DependsNode> &nodes,
                               const std::string &ref,
                               const std::string &kind)
{
    auto iter = std::find_if(nodes.begin(), nodes.end(), [&ref](const DependsNode &node) {
        return node.ref == ref;
    });
    if (iter != nodes.end()) {
        if (iter->kind.empty()) {
            iter->kind = kind;
        }
        return *iter;
    }

    nodes.push_back(DependsNode{ .ref = ref, .kind = kind });
    return nodes.back();
}

void sortDependsTree(std::vector<DependsNode> &nodes)
{
    std::sort(nodes.begin(), nodes.end(), [](const DependsNode &lhs, const DependsNode &rhs) {
        auto kindRank = [](const std::string &kind) {
            if (kind == "base") {
                return 0;
            }
            if (kind == "runtime") {
                return 1;
            }
            if (kind == "app") {
                return 2;
            }
            if (kind == "extension") {
                return 3;
            }
            return 4;
        };

        auto lhsRank = kindRank(lhs.kind);
        auto rhsRank = kindRank(rhs.kind);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        return lhs.ref < rhs.ref;
    });

    for (auto &node : nodes) {
        sortDependsTree(node.children);
    }
}

} // namespace

namespace linglong::cli {

void Cli::onTaskPropertiesChanged(
  const QString &interface,                                   // NOLINT
  const QVariantMap &changed_properties,                      // NOLINT
  [[maybe_unused]] const QStringList &invalidated_properties) // NOLINT
{
    if (interface != task->interface()) {
        return;
    }

    const auto previousState = taskState.state;
    for (auto entry = changed_properties.cbegin(); entry != changed_properties.cend(); ++entry) {
        const auto &key = entry.key();
        const auto &value = entry.value();

        if (key == "State") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                LogE("dbus ipc error, State couldn't convert to int");
                continue;
            }

            taskState.state = static_cast<api::types::v1::State>(val);
            continue;
        }

        if (key == "Percentage") {
            bool ok{ false };
            auto val = value.toDouble(&ok);
            if (!ok) {
                LogE("dbus ipc error, Percentage couldn't convert to int");
                continue;
            }

            taskState.percentage = val > 100 ? 100 : val;
            continue;
        }

        if (key == "Message") {
            if (!value.canConvert<QString>()) {
                LogE("dbus ipc error, Message couldn't convert to QString");
                continue;
            }

            taskState.message = value.toString().toStdString();
            continue;
        }

        if (key == "Code") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                LogE("dbus ipc error, Code couldn't convert to int");
                continue;
            }

            taskState.errorCode = static_cast<utils::error::ErrorCode>(val);
        }
    }

    handleTaskState(previousState);
}

void Cli::interaction(const QDBusObjectPath &object_path,
                      int messageID,
                      const QVariantMap &additionalMessage)
{
    LINGLONG_TRACE("interactive with user")
    if (object_path.path() != taskObjectPath) {
        return;
    }

    auto messageType = static_cast<api::types::v1::InteractionMessageType>(messageID);
    auto msg = common::serialize::fromQVariantMap<
      api::types::v1::PackageManager1RequestInteractionAdditionalMessage>(additionalMessage);

    std::vector<std::string> actions{ "yes", "Yes", "no", "No" };

    api::types::v1::InteractionRequest req;
    req.actions = actions;
    req.summary = "Package Manager needs to confirm request.";

    switch (messageType) {
    case api::types::v1::InteractionMessageType::Upgrade: {
        auto tips =
          QString("The lower version %1 is currently installed. Do you "
                  "want to continue installing the latest version %2?")
            .arg(QString::fromStdString(msg->localRef), QString::fromStdString(msg->remoteRef));
        req.body = tips.toStdString();
    } break;
    case api::types::v1::InteractionMessageType::Downgrade:
    case api::types::v1::InteractionMessageType::Install:
    case api::types::v1::InteractionMessageType::Uninstall:
        [[fallthrough]];
    case api::types::v1::InteractionMessageType::Unknown:
        // reserve for future use
        req.body = "unknown interaction type";
        break;
    }

    std::string action;
    auto notifyReply = notifier->request(req);
    if (!notifyReply) {
        LogE("internal error: notify failed");
        action = "no";
    } else {
        action = notifyReply->action.value();
    }

    // FIXME: if the notifier is a DummyNotifier, treat the action as yes.(for deepin-app-store)
    // But this behavior is no correct. We should treat it as no and tell people to add additional
    // option '-y'.
    if (action == "Y" || action == "y" || action == "Yes" || action == "yes" || action == "dummy") {
        action = "yes";
    } else {
        action = "no";
    }

    LogD("action: {}", action);

    auto reply = api::types::v1::InteractionReply{ .action = action };
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return;
    }

    QDBusPendingReply<void> dbusReply =
      (*pkgMan)->ReplyInteraction(object_path, common::serialize::toQVariantMap(reply));
    dbusReply.waitForFinished();
    if (dbusReply.isError()) {
        this->printer.printErr(
          LINGLONG_ERRV(dbusReply.error().message().toStdString(), dbusReply.error().type()));
    }
}

void Cli::onTaskAdded(const QDBusObjectPath &object_path)
{
    LogD("task added: {}", object_path.path().toStdString());
}

void Cli::onTaskRemoved(const QDBusObjectPath &object_path,
                        int state,
                        int code,
                        const QString &message)
{
    LogD("task removed: {}", object_path.path().toStdString());
    if (object_path.path() != taskObjectPath) {
        return;
    }

    if (task) {
        if (auto pkgMan = this->getPkgMan()) {
            (*pkgMan)->connection().disconnect(
              (*pkgMan)->service(),
              taskObjectPath,
              "org.freedesktop.DBus.Properties",
              "PropertiesChanged",
              this,
              SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)));
        }

        onTaskPropertiesChanged(task->interface(),
                                {
                                  { "State", state },
                                  { "Code", code },
                                  { "Message", message },
                                },
                                {});
    }

    delete task;
    task = nullptr;
    Q_EMIT taskDone();
}

void Cli::handleTaskState(api::types::v1::State previousState) noexcept
{
    if (taskState.state == api::types::v1::State::Unknown) {
        LogW("task state is unknown");
        return;
    }

    if (taskState.state == api::types::v1::State::Failed
        || taskState.state == api::types::v1::State::Canceled) {
        if (taskState.state != previousState) {
            this->printer.clearLine();
            this->printOnTaskFailed();
        }
        return;
    }

    if (taskState.state == api::types::v1::State::Succeed) {
        if (taskState.state != previousState) {
            this->printer.clearLine();
            this->printOnTaskSuccess();
        }
        return;
    }

    if (!this->globalOptions.noProgress) {
        this->printer.printProgress(taskState.percentage, taskState.message);
    }
}

void Cli::printOnTaskFailed()
{
    LINGLONG_TRACE("cli handle task failed");

    auto error = LINGLONG_ERRV(taskState.message, taskState.errorCode);

    switch (taskState.taskType) {
    case TaskType::Install:
        handleInstallError(
          error,
          std::get<api::types::v1::PackageManager1InstallParameters>(taskState.params));
        break;
    case TaskType::InstallFromFile:
        handleInstallFromFileError(error);
        break;
    case TaskType::Uninstall:
        handleUninstallError(error);
        break;
    case TaskType::Upgrade:
        handleUpgradeError(error);
        break;
    default:
        handleCommonError(error);
        break;
    }
}

void Cli::printOnTaskSuccess()
{
    this->printer.printMessage(taskState.message);
}

Cli::Cli(Printer &printer,
         ocppi::cli::CLI &ociCLI,
         runtime::ContainerBuilder &containerBuilder,
         bool peerMode,
         std::unique_ptr<InteractiveNotifier> &&notifier,
         QObject *parent)
    : QObject(parent)
    , printer(printer)
    , ociCLI(ociCLI)
    , containerBuilder(containerBuilder)
    , notifier(std::move(notifier))
    , peerMode(peerMode)
{
}

utils::error::Result<repo::OSTreeRepo *> Cli::getRepo(bool forceReload) noexcept
{
    LINGLONG_TRACE("get local repo");

    if (this->repository && !forceReload) {
        return this->repository.get();
    }

    auto repo = this->loadRepoFromPath(LINGLONG_ROOT);
    if (!repo) {
        LogD("failed to load repo, try to initialize repo via package manager: {}", repo.error());

        auto initRepo = this->initializeRepo();
        if (!initRepo) {
            return LINGLONG_ERR(initRepo);
        }

        repo = this->loadRepoFromPath(LINGLONG_ROOT);
        if (!repo) {
            return LINGLONG_ERR(repo);
        }
    }

    this->repository = std::move(repo).value();
    return this->repository.get();
}

utils::error::Result<std::unique_ptr<repo::OSTreeRepo>>
Cli::loadRepoFromPath(const std::filesystem::path &repoRoot) noexcept
{
    LINGLONG_TRACE("load repo from path");

    return repo::OSTreeRepo::loadFromPath(repoRoot);
}

utils::error::Result<void> Cli::initializeRepo() noexcept
{
    LINGLONG_TRACE("initialize repo");

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR("failed to initialize repo via package manager", pkgMan);
    }

    return LINGLONG_OK;
}

utils::error::Result<api::dbus::v1::PackageManager *> Cli::getPkgMan()
{
    LINGLONG_TRACE("get package manager");

    if (this->pkgMan) {
        return this->pkgMan.get();
    }

    auto pkgMan = this->peerMode ? this->initializePeerModePackageManager()
                                 : this->initializeDBusPackageManager();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }
    auto pkgManProxy = std::move(*pkgMan);

    auto conn = pkgManProxy->connection();
    if (!conn.connect(pkgManProxy->service(),
                      pkgManProxy->path(),
                      pkgManProxy->interface(),
                      "TaskAdded",
                      this,
                      SLOT(onTaskAdded(QDBusObjectPath)))) {
        return LINGLONG_ERR("couldn't connect to package manager signal 'TaskAdded'");
    }

    if (!conn.connect(pkgManProxy->service(),
                      pkgManProxy->path(),
                      pkgManProxy->interface(),
                      "TaskRemoved",
                      this,
                      SLOT(onTaskRemoved(QDBusObjectPath, int, int, QString)))) {
        return LINGLONG_ERR("couldn't connect to package manager signal 'TaskRemoved'");
    }

    if (!conn.connect(pkgManProxy->service(),
                      pkgManProxy->path(),
                      pkgManProxy->interface(),
                      "RequestInteraction",
                      this,
                      SLOT(interaction(QDBusObjectPath, int, QVariantMap)))) {
        return LINGLONG_ERR(fmt::format("Failed to connect signal RequestInteraction: {}",
                                        conn.lastError().message().toStdString()));
    }

    pkgManProxy->setTimeout(INT_MAX);

    this->pkgMan = std::move(pkgManProxy);
    return this->pkgMan.get();
}

utils::error::Result<std::unique_ptr<api::dbus::v1::PackageManager>>
Cli::initializePeerModePackageManager()
{
    LINGLONG_TRACE("initialize peer mode package manager");

    if (getuid() != 0) {
        return LINGLONG_ERR("--no-dbus should only be used by root user.");
    }

    auto socketDirRet = preparePeerSocketDir();
    if (!socketDirRet) {
        return LINGLONG_ERR("failed to prepare peer socket directory", std::move(socketDirRet));
    }

    const auto socketDir = std::move(socketDirRet).value();
    auto removePeerSocketDir = linglong::utils::finally::finally([&socketDir] {
        std::error_code ec;
        std::filesystem::remove_all(socketDir, ec);
        if (ec) {
            LogW("failed to remove peer socket directory {}: {}", socketDir.string(), ec.message());
        }
    });

    const auto socketPath = socketDir / "package-manager.socket";
    const auto socketPathString = socketPath.string();
    const auto pkgManAddressString = "unix:path=" + socketPathString;
    auto started = QProcess::startDetached("sudo",
                                           { "--user",
                                             LINGLONG_USERNAME,
                                             "--preserve-env=QT_FORCE_STDERR_LOGGING",
                                             "--preserve-env=QDBUS_DEBUG",
                                             LINGLONG_LIBEXEC_DIR "/ll-package-manager",
                                             "--no-dbus",
                                             "--peer-socket",
                                             QString::fromStdString(socketPathString) });
    if (!started) {
        return LINGLONG_ERR("Failed to start ll-package-manager");
    }

    QDBusConnection pkgManConn("ll-package-manager");
    using namespace std::chrono_literals;
    for (int retry = 0; retry < 50 && !pkgManConn.isConnected(); ++retry) {
        QDBusConnection::disconnectFromPeer("ll-package-manager");
        std::this_thread::sleep_for(200ms);
        pkgManConn = QDBusConnection::connectToPeer(QString::fromStdString(pkgManAddressString),
                                                    "ll-package-manager");
    }

    if (!pkgManConn.isConnected()) {
        return LINGLONG_ERR(fmt::format("Failed to connect to ll-package-manager: {}",
                                        pkgManConn.lastError().message().toStdString()));
    }

    auto peerReady = waitForDBusPeerReady("", "/org/deepin/linglong/PackageManager1", pkgManConn);
    if (!peerReady) {
        return LINGLONG_ERR("Failed to initialize peer connection", peerReady);
    }

    std::error_code ec;
    std::filesystem::remove(socketPath, ec);
    if (ec) {
        LogW("failed to remove peer package manager socket {}: {}",
             socketPath.string(),
             ec.message());
    }

    return std::make_unique<api::dbus::v1::PackageManager>("",
                                                           "/org/deepin/linglong/PackageManager1",
                                                           pkgManConn,
                                                           QCoreApplication::instance());
}

utils::error::Result<std::unique_ptr<api::dbus::v1::PackageManager>>
Cli::initializeDBusPackageManager()
{
    LINGLONG_TRACE("initialize dbus package manager");

    const auto &pkgManConn = QDBusConnection::systemBus();

    auto peerReady = waitForDBusPeerReady("org.deepin.linglong.PackageManager1",
                                          "/org/deepin/linglong/PackageManager1",
                                          pkgManConn);
    if (!peerReady) {
        return LINGLONG_ERR("Failed to activate org.deepin.linglong.PackageManager1", peerReady);
    }

    return std::make_unique<api::dbus::v1::PackageManager>("org.deepin.linglong.PackageManager1",
                                                           "/org/deepin/linglong/PackageManager1",
                                                           pkgManConn,
                                                           QCoreApplication::instance());
}

int Cli::run(const RunOptions &options)
{
    LINGLONG_TRACE("command run");

    auto userContainerDir = std::filesystem::path{ "/run/linglong" } / std::to_string(getuid());
    if (auto ret = utils::ensureDirectory(userContainerDir); !ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    auto pidFile = userContainerDir / std::to_string(getpid());
    // placeholder file
    auto fd = ::open(pidFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd == -1) {
        LogE("create file {} error: {}", pidFile, common::error::errorString(errno));
        QCoreApplication::exit(-1);
        return -1;
    }
    ::close(fd);

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [pidFile] {
        std::error_code ec;
        if (!std::filesystem::remove(pidFile, ec) && ec) {
            LogE("failed to remove file {}: {}", pidFile.c_str(), ec.message());
        }
    });

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto curAppRef = (*repo)->clearReferenceLocal(*fuzzyRef);
    if (!curAppRef) {
        this->printer.printErr(curAppRef.error());
        return -1;
    }

    auto loaded = linglong::utils::loadRuntimeConfig(options.appid, options.instance.value_or(""));
    if (!loaded) {
        this->printer.printErr(loaded.error());
        return -1;
    }
    auto runtimeConfig = std::move(loaded).value();

    linglong::runtime::ResolveOptions opts;
    auto resolveOptionsRes = opts.applyOptions(runtimeConfig, options);
    if (!resolveOptionsRes) {
        this->printer.printErr(resolveOptionsRes.error());
        return -1;
    }
    if (options.debug) {
        opts.instance = makeDebugInstanceID();
    }

    bool nvidiaCdiFound = false;
    if (opts.cdiDevices) {
        nvidiaCdiFound = std::any_of(opts.cdiDevices->begin(),
                                     opts.cdiDevices->end(),
                                     [](const api::types::v1::CdiDeviceEntry &device) {
                                         return device.kind == "nvidia.com/gpu";
                                     });
    }

    if (!nvidiaCdiFound) {
        detectDrivers();
    }

    auto runContext = std::make_unique<runtime::RunContext>(**repo);
    auto res = runContext->resolve(*curAppRef, opts);
    if (!res) {
        handleCommonError(res.error());
        return -1;
    }

    if (options.debug) {
        auto installRes = ensureBaseDevelopModule(*runContext);
        if (!installRes) {
            this->printer.printErr(installRes.error());
            return -1;
        }

        repo = this->getRepo(true);
        if (!repo) {
            this->printer.printErr(repo.error());
            return -1;
        }

        runContext = std::make_unique<runtime::RunContext>(**repo);
        res = runContext->resolve(*curAppRef, opts);
        if (!res) {
            handleCommonError(res.error());
            return -1;
        }
    }

    auto runContextCfg = runContext->getConfig();
    LogD("RunContext Config:\n{}", nlohmann::json(runContextCfg).dump());

    auto containerID = runContext->getContainerId();
    LogD("run {} with container id: {}", curAppRef->toString(), containerID);

    auto targetItem = runContext->getCachedTargetItem();
    if (!targetItem) {
        this->printer.printErr(LINGLONG_ERRV("failed to get cached target item", targetItem));
        return -1;
    }
    const auto &info = targetItem->info;

    auto commands = options.commands;
    if (options.commands.empty()) {
        commands = info.command.value_or(std::vector<std::string>{ "bash" });
    }
    commands = filePathMapping(commands, options);

    // this lambda will dump reference of containerID, app, base and runtime to
    // /run/linglong/getuid()/getpid() to store these needed information
    auto dumpContainerInfo = [&pidFile, &runContext, this]() -> bool {
        LINGLONG_TRACE("dump info")
        std::error_code ec;
        if (!std::filesystem::exists(pidFile, ec)) {
            if (ec) {
                LogE("couldn't get status of {}: {}", pidFile.c_str(), ec.message());
                return false;
            }

            LogE("state file {} doesn't exist", pidFile.string());
            return false;
        }

        std::ofstream stream{ pidFile };
        if (!stream.is_open()) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("failed to open {}", pidFile.c_str())));
            return false;
        }
        stream << nlohmann::json(runContext->stateInfo());
        stream.close();

        return true;
    };

    auto containers = getCurrentContainers().value_or(std::vector<api::types::v1::CliContainer>{});
    for (const auto &container : containers) {
        LogD("found running container: {}", container.package);
        if (container.id != containerID || container.package != curAppRef->toString()) {
            continue;
        }

        if (!dumpContainerInfo()) {
            return -1;
        }

        if (delegateToContainerInit(container.id, commands)) {
            return 0;
        }

        // fallback to run
        break;
    }

    auto cacheRes = this->ensureCache(*runContext);
    if (!cacheRes) {
        this->printer.printErr(LINGLONG_ERRV(cacheRes));
        return -1;
    }

    if (!dumpContainerInfo()) {
        return -1;
    }

    auto namespaceRes = linglong::utils::needRunInNamespace();
    if (!namespaceRes) {
        this->printer.printErr(namespaceRes.error());
        return -1;
    }

    if (*namespaceRes) {
        const auto qtArgs = QCoreApplication::arguments();
        auto selfExe = linglong::utils::getSelfExe();
        if (!selfExe) {
            this->printer.printErr(selfExe.error());
            return -1;
        }

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(qtArgs.size()) + 2);
        for (const auto &arg : qtArgs) {
            args.emplace_back(arg.toStdString());
        }
        args[0] = std::move(*selfExe);

        auto insertPos = std::find(args.begin(), args.end(), std::string{ "run" });
        if (insertPos == args.end()) {
            this->printer.printErr(LINGLONG_ERRV("failed to locate run subcommand"));
            return -1;
        }

        auto contextJson = nlohmann::json(runContext->getConfig()).dump();
        args.insert(insertPos + 1, { "--run-context", contextJson });

        std::vector<char *> argPointers;
        argPointers.reserve(args.size() + 1);
        for (auto &arg : args) {
            argPointers.push_back(arg.data());
        }
        argPointers.push_back(nullptr);

        auto runRes = linglong::utils::runInNamespace(static_cast<int>(args.size()),
                                                      argPointers.data(),
                                                      geteuid(),
                                                      getegid());
        if (!runRes) {
            this->printer.printErr(runRes.error());
            return -1;
        }

        return *runRes;
    }

    return this->runResolvedContext(*runContext, options, std::move(runtimeConfig));
}

int Cli::runWithContext(const RunOptions &options)
{
    LINGLONG_TRACE("command run with context");

    if (!options.runContext) {
        this->printer.printErr(LINGLONG_ERRV("run context is required"));
        return -1;
    }

    auto loaded = linglong::utils::loadRuntimeConfig(options.appid, options.instance.value_or(""));
    if (!loaded) {
        this->printer.printErr(loaded.error());
        return -1;
    }
    auto runtimeConfig = std::move(loaded).value();

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    runtime::RunContext runContext(**repo);
    try {
        auto cfg =
          nlohmann::json::parse(*options.runContext).get<api::types::v1::RunContextConfig>();
        auto res = runContext.resolve(cfg);
        if (!res) {
            this->printer.printErr(res.error());
            return -1;
        }
    } catch (const std::exception &e) {
        this->printer.printErr(
          LINGLONG_ERRV(fmt::format("failed to parse run context: {}", e.what())));
        return -1;
    }

    return this->runResolvedContext(runContext, options, std::move(runtimeConfig));
}

utils::error::Result<void> Cli::ensureBaseDevelopModule(runtime::RunContext &runContext)
{
    LINGLONG_TRACE("ensure base develop module");

    const auto &baseLayer = runContext.getBaseLayer();
    if (!baseLayer) {
        return LINGLONG_ERR("run context has no base layer");
    }

    const auto &baseRef = baseLayer->getReference();
    auto modules = runContext.getRepo().getModuleList(baseRef);
    if (std::find(modules.begin(), modules.end(), DebugDevelopModule) != modules.end()) {
        return LINGLONG_OK;
    }

    this->printer.printMessage(
      fmt::format(_("Base {} has no develop module installed, installing it now."),
                  baseRef.toString()));

    auto installResult = this->install(InstallOptions{
      .appid = baseRef.toString(),
      .module = DebugDevelopModule,
    });
    if (installResult != 0) {
        return LINGLONG_ERR(
          fmt::format("failed to install develop module for base {}", baseRef.toString()));
    }

    return LINGLONG_OK;
}

int Cli::runResolvedContext(runtime::RunContext &runContext,
                            const RunOptions &options,
                            std::optional<api::types::v1::RuntimeConfigure> runtimeConfig)
{
    LINGLONG_TRACE("run resolved context");

    auto targetItem = runContext.getCachedTargetItem();
    if (!targetItem) {
        this->printer.printErr(LINGLONG_ERRV("failed to get cached target item", targetItem));
        return -1;
    }

    auto debugOptions = options;
    if (debugOptions.debug) {
        debugOptions.debugSymbolDir =
          mergeDebugSymbolDir(debugOptions.debugSymbolDir, makeDebugSymbolDir(runContext));
    }

    auto commands = debugOptions.commands;
    if (options.commands.empty()) {
        commands = targetItem->info.command.value_or(std::vector<std::string>{ "bash" });
    }
    commands = filePathMapping(commands, debugOptions);
    if (debugOptions.debug) {
        commands = makeDebugCommand(debugOptions, std::move(commands));
    }

    auto appCache =
      common::dir::getContainerCacheDir(targetItem->commit, runContext.getContainerId());

    runtime::RunContainerOptions runOptions;
    runOptions.enableSecurityContext(runtime::getDefaultSecurityContexts());
    runOptions.common.containerCachePath = appCache;
    if (runtimeConfig) {
        auto runtimeConfigRes = runOptions.applyRuntimeConfig(*runtimeConfig);
        if (!runtimeConfigRes) {
            this->printer.printErr(runtimeConfigRes.error());
            return -1;
        }
    }
    auto res = runOptions.applyCliRunOptions(debugOptions);
    if (!res) {
        this->printer.printErr(res.error());
        return -1;
    }

    const auto &appid = runContext.getTargetID();
    if (!options.disableXdp.has_value() && !utils::isValidXdgDesktopPortalId(appid)) {
        LogW("appid '{}' doesn't conform to XDP ID specification, disabling XDP integration. "
             "Use --enable-xdp to override.",
             appid);
        runOptions.disableXdp = true;
    }

    auto container = this->containerBuilder.createRunContainer(runContext, runOptions);
    if (!container) {
        this->printer.printErr(container.error());
        return -1;
    }

    auto process = ocppi::runtime::config::types::Process{ .args = std::move(commands) };
    if (debugOptions.debug) {
        printDebugAttachHint(debugOptions);
    }
    if (!debugOptions.workdir.value_or("").empty()) {
        auto workdir = std::filesystem::path(debugOptions.workdir.value());
        if (!workdir.is_absolute()) {
            auto msg = fmt::format("Workdir must be an absolute path: {}", workdir);
            this->printer.printErr(LINGLONG_ERRV(msg));
            return -1;
        }
        process.cwd = workdir;
    }

    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    return 0;
}

int Cli::enter(const EnterOptions &options)
{
    LINGLONG_TRACE("ll-cli exec");
    auto containerIDList = this->getRunningAppContainers(options.instance);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    auto containerID = containerIDList->front();
    LogI("select container id: {}", containerID);
    auto commands = options.commands;
    if (commands.empty()) {
        commands = utils::BashCommandHelper::generateDefaultBashCommand();
    }

    auto opt = ocppi::runtime::ExecOption{
        .uid = ::getuid(),
        .gid = ::getgid(),
    };

    auto result =
      this->ociCLI.exec(containerID, commands[0], { commands.begin() + 1, commands.end() }, opt);
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

utils::error::Result<std::vector<api::types::v1::CliContainer>>
Cli::getCurrentContainers() const noexcept
{
    LINGLONG_TRACE("get current running containers")

    auto containersRet = this->ociCLI.list();
    if (!containersRet) {
        return LINGLONG_ERR(containersRet);
    }
    auto containers = std::move(containersRet).value();

    std::vector<api::types::v1::CliContainer> myContainers;
    auto infoDir = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());

    std::error_code ec;
    auto it = std::filesystem::directory_iterator{ infoDir, ec };
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            return myContainers;
        }

        return LINGLONG_ERR(fmt::format("failed to list {}", infoDir), ec);
    }

    for (const auto &pidFile : it) {
        const auto &file = pidFile.path();
        const auto &process = "/proc" / file.filename();

        std::error_code ec;
        if (!std::filesystem::exists(process, ec)) {
            // this process may exit abnormally, skip it.
            LogD("{} doesn't exist", process.string());
            continue;
        }

        if (pidFile.file_size(ec) == 0) {
            continue;
        }

        auto info = linglong::utils::serialize::LoadJSONFile<
          linglong::api::types::v1::ContainerProcessStateInfo>(file);
        if (!info) {
            LogD("load info from {}: {}", file.string(), info.error());
            continue;
        }

        auto container = std::find_if(containers.begin(),
                                      containers.end(),
                                      [&info](const ocppi::types::ContainerListItem &item) {
                                          return item.id == info->containerID;
                                      });
        if (container == containers.cend()) {
            LogD("couldn't find container that process {} belongs to", file.filename().string());
            continue;
        }

        myContainers.emplace_back(api::types::v1::CliContainer{
          .id = std::move(info->containerID),
          .package = !info->app.empty()
            ? info->app
            : (info->runtime && !info->runtime->empty() ? *info->runtime : info->base),
          .pid = container->pid,
        });
    }

    return myContainers;
}

int Cli::ps(const PsOptions &options)
{
    auto myContainers = getCurrentContainers();
    if (!myContainers) {
        this->printer.printErr(myContainers.error());
        return -1;
    }

    if (!options.noTruncate) {
        std::for_each(myContainers->begin(),
                      myContainers->end(),
                      [](api::types::v1::CliContainer &container) {
                          container.id = container.id.substr(0, ContainerIDDisplayLength);
                      });
    }

    this->printer.printContainers(*myContainers);

    return 0;
}

bool Cli::isContainerIDMatch(const std::string &containerID, const std::string &shortID)
{
    if (containerID == shortID) {
        return true;
    }

    if (shortID.size() < ContainerIDDisplayLength) {
        return false;
    }

    return common::strings::starts_with(containerID, shortID);
}

utils::error::Result<std::vector<std::string>> Cli::getRunningAppContainers(const std::string &id)
{
    LINGLONG_TRACE("get app running containers");

    std::vector<std::string> containerIDList{};
    auto containers = getCurrentContainers();
    if (!containers) {
        return LINGLONG_ERR(containers);
    }

    for (const auto &container : *containers) {
        // first check if the id matches container id, then check if the id matches package appid or
        // reference
        if (isContainerIDMatch(container.id, id)) {
            containerIDList.emplace_back(container.id);
            continue;
        }

        auto ref = package::Reference::parse(container.package);
        if (!ref) {
            LogW("{}", ref.error());
            continue;
        }

        if (ref->id == id || ref->toString() == id) {
            containerIDList.emplace_back(container.id);
        }
    }

    if (containerIDList.size() > 1) {
        auto msg =
          fmt::format("multiple running containers match the specified identifier '{}': {}. "
                      "Please specify a more specific identifier.",
                      id,
                      containerIDList);
        return LINGLONG_ERR(msg);
    }

    return containerIDList;
}

int Cli::kill(const KillOptions &options)
{
    LINGLONG_TRACE("command kill");

    auto containerIDList = this->getRunningAppContainers(options.appid);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    auto ret = 0;
    for (const auto &containerID : *containerIDList) {
        LogI("select container id {}", containerID);
        auto result = this->ociCLI.kill(ocppi::runtime::ContainerID(containerID),
                                        ocppi::runtime::Signal(options.signal));
        if (!result) {
            auto err = LINGLONG_ERRV(result);
            this->printer.printErr(err);
            ret = -1;
        }
    }

    return ret;
}

void Cli::cancelCurrentTask()
{
    if (this->task != nullptr) {
        LogD("cancel running task");
        this->task->Cancel();
    }
}

int Cli::installFromFile(const QFileInfo &fileInfo,
                         const api::types::v1::CommonOptions &commonOptions)
{
    auto filePath = fileInfo.absoluteFilePath();
    LINGLONG_TRACE(fmt::format("install from file {}", filePath.toStdString()));

    LogI("install from file {}", filePath.toStdString());
    QFile file{ filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        auto err = LINGLONG_ERR(file.errorString().toStdString());
        this->printer.printErr(err.value());
        return -1;
    }

    QDBusUnixFileDescriptor dbusFileDescriptor(file.handle());

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto caps = (*pkgMan)->connection().connectionCapabilities();
    if (!(caps & QDBusConnection::UnixFileDescriptorPassing)) {
        this->printer.printErr(LINGLONG_ERRV("peer connection does not support Unix FD passing"));
        return -1;
    }

    auto pendingReply = (*pkgMan)->InstallFromFile(dbusFileDescriptor,
                                                   fileInfo.suffix(),
                                                   common::serialize::toQVariantMap(commonOptions));
    auto res = waitTaskCreated(pendingReply, TaskType::InstallFromFile);
    if (!res) {
        this->handleInstallFromFileError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::install(const InstallOptions &options)
{
    LINGLONG_TRACE("command install");

    auto params =
      api::types::v1::PackageManager1InstallParameters{ .options = { .force = false,
                                                                     .skipInteraction = false } };
    params.options.force = options.forceOpt;
    params.options.skipInteraction = options.confirmOpt;
    params.repo = options.repo;

    QFileInfo info(QString::fromStdString(options.appid));

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(QFileInfo{ info.absoluteFilePath() }, params.options);
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }

    if (!options.module.empty()) {
        params.package.modules = { options.module };
    } else {
        params.package.modules = getAutoModuleList();
    }

    LogD("install module: {}", common::strings::join(*params.package.modules));

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Install(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Install);
    if (!res) {
        handleInstallError(res.error(), params);
        return -1;
    }
    this->taskState.params = std::move(params);

    waitTaskDone();

    updateAM();
    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::upgrade(const UpgradeOptions &options)
{
    LINGLONG_TRACE("command upgrade");

    std::vector<package::Reference> toUpgrade;
    if (!options.appid.empty()) {
        auto fuzzyRef = package::FuzzyReference::parse(options.appid);
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto repo = this->getRepo();
        if (!repo) {
            this->printer.printErr(repo.error());
            return -1;
        }

        auto localRef = (*repo)->clearReferenceLocal(*fuzzyRef);
        if (!localRef) {
            this->printer.printMessage(
              fmt::format(_("Application {} is not installed."), options.appid));
            return -1;
        }

        auto layerItemRet = (*repo)->getLayerItem(*localRef);
        if (!layerItemRet) {
            this->printer.printErr(layerItemRet.error());
            return -1;
        }
        if (layerItemRet->info.kind != "app") {
            this->printer.printMessage(fmt::format(_("{} is not an application."), options.appid));
            return -1;
        }
        toUpgrade.emplace_back(std::move(localRef).value());
    }

    api::types::v1::PackageManager1UpdateParameters params;
    params.appOnly = options.appOnly;
    params.depsOnly = options.depsOnly;
    for (const auto &ref : toUpgrade) {
        api::types::v1::PackageManager1Package package;
        package.id = ref.id;
        package.channel = ref.channel;
        params.packages.emplace_back(std::move(package));
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Update(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Upgrade);
    if (!res) {
        handleUpgradeError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::search(const SearchOptions &options)
{
    LINGLONG_TRACE("command search");

    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = options.appid,
        .repos = {},
    };

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto repoConfig = (*repo)->getOrderedConfig();
    if (repoConfig.repos.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no repo found"));
        return -1;
    }

    if (options.repo) {
        auto it = std::find_if(repoConfig.repos.begin(),
                               repoConfig.repos.end(),
                               [&options](const api::types::v1::Repo &repo) {
                                   return repo.alias.value_or(repo.name) == options.repo.value();
                               });
        if (it == repoConfig.repos.end()) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("repo {} not found", options.repo.value())));
            return -1;
        }
        params.repos.emplace_back(options.repo.value());
    } else {
        // search all repos
        for (const auto &repo : repoConfig.repos) {
            params.repos.emplace_back(repo.alias.value_or(repo.name));
        }
    }

    std::optional<QString> pendingJobID;

    QEventLoop loop;
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    connect(
      *pkgMan,
      &api::dbus::v1::PackageManager::SearchFinished,
      [&pendingJobID, this, &loop, &options](const QString &jobID, const QVariantMap &data) {
          LINGLONG_TRACE("process search result");
          // Note: once an error occurs, remember to return after exiting the loop.
          if (!pendingJobID || *pendingJobID != jobID) {
              return;
          }
          auto result =
            common::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(data);
          if (!result) {
              this->printer.printErr(result.error());
              loop.exit(-1);
              return;
          }
          // Note: should check return code of PackageManager1SearchResult
          auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
          if (resultCode != utils::error::ErrorCode::Success) {
              if (resultCode == utils::error::ErrorCode::Failed) {
                  this->printer.printErr(LINGLONG_ERRV("\n" + result->message, result->code));
                  loop.exit(result->code);
                  return;
              }

              if (resultCode == utils::error::ErrorCode::NetworkError) {
                  this->printer.printMessage(_("Network connection failed. Please:"
                                               "\n1. Check your internet connection"
                                               "\n2. Verify network proxy settings if used"));
              }

              if (this->globalOptions.verbose) {
                  this->printer.printErr(LINGLONG_ERRV("\n" + result->message, result->code));
              }

              loop.exit(result->code);
              return;
          }

          if (!result->packages) {
              this->printer.printPackages({});
              loop.exit(0);
              return;
          }

          auto allPackages = std::move(result->packages).value();
          if (!options.showDevel) {
              std::for_each(allPackages.begin(),
                            allPackages.end(),
                            [](decltype(allPackages)::reference pkgs) {
                                auto &vec = pkgs.second;

                                auto it =
                                  std::remove_if(vec.begin(),
                                                 vec.end(),
                                                 [](const api::types::v1::PackageInfoV2 &pkg) {
                                                     return pkg.packageInfoV2Module == "develop";
                                                 });
                                vec.erase(it, vec.end());
                            });
          }

          if (!options.type.empty()) {
              filterPackageInfosByType(allPackages, options.type);
          }

          // default only the latest version is displayed
          if (!options.showAllVersion) {
              filterPackageInfosByVersion(allPackages);
          }

          this->printer.printSearchResult(allPackages);
          loop.exit(0);
      });

    auto pendingReply = (*pkgMan)->Search(common::serialize::toQVariantMap(params));
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    pendingJobID = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::prune()
{
    LINGLONG_TRACE("command prune");

    QEventLoop loop;
    QString jobIDReply = "";
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    connect(*pkgMan,
            &api::dbus::v1::PackageManager::PruneFinished,
            [this, &loop, &jobIDReply](const QString &jobID, const QVariantMap &data) {
                LINGLONG_TRACE("process prune result");
                if (jobIDReply != jobID) {
                    return;
                }
                auto ret =
                  common::serialize::fromQVariantMap<api::types::v1::PackageManager1PruneResult>(
                    data);
                if (!ret) {
                    this->printer.printErr(ret.error());
                    loop.exit(-1);
                    return;
                }

                if (!ret->packages) {
                    this->printer.printErr(LINGLONG_ERRV("No packages to prune."));
                    loop.exit(0);
                    return;
                }

                this->printer.printPruneResult(*ret->packages);
                loop.exit(0);
            });

    auto pendingReply = (*pkgMan)->Prune();
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }
    jobIDReply = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::uninstall(const UninstallOptions &options)
{
    LINGLONG_TRACE("command uninstall");

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto params = api::types::v1::PackageManager1UninstallParameters{};
    params.options = api::types::v1::CommonOptions{
        .force = options.forceOpt,
        .skipInteraction = false,
    };
    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }
    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Uninstall(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Uninstall);
    if (!res) {
        this->handleUninstallError(res.error());
        return -1;
    }

    waitTaskDone();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::list(const ListOptions &options)
{
    if (options.showUpgradeList) {
        auto upgradeList = this->listUpgradable();
        if (!upgradeList) {
            this->printer.printErr(upgradeList.error());
            return -1;
        }
        // 按id排序
        std::sort(upgradeList->begin(), upgradeList->end(), [](const auto &lhs, const auto &rhs) {
            return lhs.id < rhs.id;
        });
        this->printer.printUpgradeList(*upgradeList);
        return 0;
    }
    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto items = (*repo)->listLayerItem();
    if (!items) {
        this->printer.printErr(items.error());
        return -1;
    }
    std::vector<api::types::v1::PackageInfoDisplay> list;
    for (const auto &item : *items) {
        nlohmann::json json = item.info;
        auto m = json.get<api::types::v1::PackageInfoDisplay>();
        auto t = (*repo)->getLayerCreateTime(item);
        if (t.has_value()) {
            m.installTime = *t;
        }
        list.push_back(std::move(m));
    }
    if (!options.type.empty()) {
        filterPackageInfosByType(list, options.type);
    }
    // 按id排序
    std::sort(list.begin(), list.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.id < rhs.id;
    });
    this->printer.printPackages(list);
    return 0;
}

int Cli::size(const SizeOptions &options)
{
    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto items = (*repo)->listLayerItem();
    if (!items) {
        this->printer.printErr(items.error());
        return -1;
    }

    std::vector<Printer::ModuleSizeInfo> moduleSizes;
    moduleSizes.reserve(items->size());
    std::vector<std::filesystem::path> modulePaths;
    modulePaths.reserve(items->size());

    for (const auto &item : *items) {
        if (item.deleted.value_or(false)) {
            continue;
        }

        auto ref = package::Reference::fromPackageInfo(item.info);
        if (!ref) {
            this->printer.printErr(ref.error());
            return -1;
        }

        auto layerDir = (*repo)->getLayerDir(*ref, item.info.packageInfoV2Module);
        if (!layerDir) {
            this->printer.printErr(layerDir.error());
            return -1;
        }

        moduleSizes.push_back(Printer::ModuleSizeInfo{
          .id = item.info.id,
          .name = item.info.name,
          .version = item.info.version,
          .channel = item.info.channel,
          .module = item.info.packageInfoV2Module,
        });
        modulePaths.push_back(layerDir->path());
    }

    auto calculatedSizes = calculateModuleSizes(modulePaths);
    if (!calculatedSizes) {
        this->printer.printErr(calculatedSizes.error());
        return -1;
    }

    for (std::size_t index = 0; index < moduleSizes.size(); ++index) {
        moduleSizes[index].exclusiveSize = calculatedSizes->moduleSizes.at(index).exclusiveSize;
        moduleSizes[index].sharedSize = calculatedSizes->moduleSizes.at(index).sharedSize;
        moduleSizes[index].logicalSize = calculatedSizes->moduleSizes.at(index).logicalSize;
        moduleSizes[index].actualSize = calculatedSizes->moduleSizes.at(index).actualSize;
    }

    std::sort(moduleSizes.begin(), moduleSizes.end(), [&options](const auto &lhs, const auto &rhs) {
        auto compareSize = [&options, &lhs, &rhs](std::uint64_t lhsSize, std::uint64_t rhsSize) {
            if (lhsSize == rhsSize) {
                return moduleNameLess(lhs, rhs);
            }

            return options.ascending ? lhsSize < rhsSize : lhsSize > rhsSize;
        };

        if (options.sortBy == "id") {
            return options.ascending ? moduleNameLess(lhs, rhs) : moduleNameLess(rhs, lhs);
        }
        if (options.sortBy == "logical") {
            return compareSize(lhs.logicalSize, rhs.logicalSize);
        }
        if (options.sortBy == "exclusive") {
            return compareSize(lhs.exclusiveSize, rhs.exclusiveSize);
        }
        if (options.sortBy == "shared") {
            return compareSize(lhs.sharedSize, rhs.sharedSize);
        }

        return compareSize(lhs.actualSize, rhs.actualSize);
    });

    auto repoSize = calculateRealDiskUsage((*repo)->getRepoDir());
    if (!repoSize) {
        this->printer.printErr(repoSize.error());
        return -1;
    }

    this->printer.printModuleSizes(moduleSizes, calculatedSizes->actualTotalSize, *repoSize);
    return 0;
}

int Cli::depends(const DependsOptions &options)
{
    LINGLONG_TRACE("command depends");

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    std::vector<package::Reference> appRefs;
    if (options.appid.empty()) {
        auto items = (*repo)->listLayerItem();
        if (!items) {
            this->printer.printErr(items.error());
            return -1;
        }

        std::unordered_set<std::string> seen;
        for (const auto &item : *items) {
            if (item.deleted.value_or(false) || item.info.kind != "app"
                || item.info.packageInfoV2Module != "binary") {
                continue;
            }

            auto ref = package::Reference::fromPackageInfo(item.info);
            if (!ref) {
                this->printer.printErr(ref.error());
                return -1;
            }

            if (seen.insert(ref->toString()).second) {
                appRefs.push_back(std::move(ref).value());
            }
        }
    } else {
        auto fuzzyRef = package::FuzzyReference::parse(options.appid);
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto appRef = (*repo)->clearReferenceLocal(*fuzzyRef);
        if (!appRef) {
            this->printer.printErr(appRef.error());
            return -1;
        }

        auto item = (*repo)->getLayerItem(*appRef);
        if (!item) {
            this->printer.printErr(item.error());
            return -1;
        }
        if (item->info.kind != "app") {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("{} is not an app", appRef->toString())));
            return -1;
        }

        appRefs.push_back(std::move(appRef).value());
    }

    std::sort(appRefs.begin(), appRefs.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.toString() < rhs.toString();
    });

    std::vector<DependsNode> trees;
    for (const auto &appRef : appRefs) {
        runtime::RunContext runContext(**repo);
        auto resolveResult = runContext.resolve(appRef);
        if (!resolveResult) {
            this->printer.printErr(resolveResult.error());
            return -1;
        }

        const auto &baseLayer = runContext.getBaseLayer();
        if (!baseLayer) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("failed to resolve base for {}", appRef.toString())));
            return -1;
        }

        auto addExtensions = [&runContext](DependsNode &targetNode, const std::string &targetRef) {
            for (const auto &extension : runContext.getExtensionLayers()) {
                const auto &extensionInfo = extension.getExtensionInfo();
                if (!extensionInfo || extensionInfo->forRef != targetRef) {
                    continue;
                }

                appendDependsNode(targetNode.children,
                                  extension.getReference().toString(),
                                  extension.getCachedItem().info.kind);
            }
        };

        const auto baseRef = baseLayer->getReference().toString();
        auto &baseNode = appendDependsNode(trees, baseRef, baseLayer->getCachedItem().info.kind);
        addExtensions(baseNode, baseRef);

        DependsNode *appParent = &baseNode;
        const auto &runtimeLayer = runContext.getRuntimeLayer();
        if (runtimeLayer) {
            const auto runtimeRef = runtimeLayer->getReference().toString();
            auto &runtimeNode = appendDependsNode(baseNode.children,
                                                  runtimeRef,
                                                  runtimeLayer->getCachedItem().info.kind);
            addExtensions(runtimeNode, runtimeRef);
            appParent = &runtimeNode;
        }

        const auto &appLayer = runContext.getAppLayer();
        if (!appLayer) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("failed to resolve app {}", appRef.toString())));
            return -1;
        }

        const auto appRefStr = appLayer->getReference().toString();
        auto &appNode =
          appendDependsNode(appParent->children, appRefStr, appLayer->getCachedItem().info.kind);
        addExtensions(appNode, appRefStr);
    }

    sortDependsTree(trees);
    this->printer.printDepends(trees);
    return 0;
}

utils::error::Result<std::vector<api::types::v1::UpgradeListResult>> Cli::listUpgradable()
{
    LINGLONG_TRACE("list upgradable");

    // only applications can be upgraded
    auto repo = this->getRepo();
    if (!repo) {
        return LINGLONG_ERR(repo);
    }

    auto upgradablePkgs = (*repo)->upgradableApps();
    if (!upgradablePkgs) {
        return LINGLONG_ERR(upgradablePkgs);
    }

    std::vector<api::types::v1::UpgradeListResult> upgradeList;
    for (const auto &pkg : *upgradablePkgs) {
        upgradeList.emplace_back(
          api::types::v1::UpgradeListResult{ .id = pkg.first.id,
                                             .newVersion = pkg.second.reference.version.toString(),
                                             .oldVersion = pkg.first.version.toString() });
    }
    return upgradeList;
}

int Cli::repo(CLI::App *app, const common::cli::RepoOptions &options)
{
    common::cli::RepoConfigBackend backend{
        .getConfig = [this]() -> utils::error::Result<api::types::v1::RepoConfigV2> {
            LINGLONG_TRACE("get repo config from package manager");

            auto pkgMan = this->getPkgMan();
            if (!pkgMan) {
                return LINGLONG_ERR(pkgMan);
            }

            auto propCfg = (*pkgMan)->configuration();
            if ((*pkgMan)->lastError().isValid()) {
                return LINGLONG_ERR((*pkgMan)->lastError().message().toStdString());
            }

            auto cfg = common::serialize::fromQVariantMap<api::types::v1::RepoConfigV2>(propCfg);
            if (!cfg) {
                return LINGLONG_ERR("failed to parse repo config", cfg.error());
            }

            return *cfg;
        },
        .setConfig = [this](const api::types::v1::RepoConfigV2 &cfg) -> utils::error::Result<void> {
            LINGLONG_TRACE("set repo config to package manager");

            auto ret = this->setRepoConfig(common::serialize::toQVariantMap(cfg));
            if (ret != 0) {
                return LINGLONG_ERR("failed to set repo config");
            }
            return LINGLONG_OK;
        },
    };

    auto ret = common::cli::handleRepoCommand(app,
                                              options,
                                              backend,
                                              { .showConfig = [this](const auto &cfg) {
                                                  this->printer.printRepoConfig(cfg);
                                              } });
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    return 0;
}

int Cli::setRepoConfig(const QVariantMap &config)
{
    LINGLONG_TRACE("set repo config");

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto reply = (*pkgMan)->SetConfiguration(config);
    reply.waitForFinished();
    if (reply.isError()) {
        auto err = LINGLONG_ERRV(reply.error().message().toStdString());
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

int Cli::info(const InfoOptions &options)
{
    LINGLONG_TRACE("command info");

    QString app = QString::fromStdString(options.appid);

    if (app.isEmpty()) {
        auto err = LINGLONG_ERR("failed to get layer path").value();
        this->printer.printErr(err);
        return err.code();
    }

    QFileInfo file(app);
    auto isLayerFile = file.isFile() && file.suffix() == "layer";

    // 如果是app，显示app 包信息
    if (!isLayerFile) {
        auto fuzzyRef = package::FuzzyReference::parse(app.toStdString());
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto repo = this->getRepo();
        if (!repo) {
            this->printer.printErr(repo.error());
            return -1;
        }

        auto ref = (*repo)->clearReferenceLocal(*fuzzyRef);
        if (!ref) {
            LogD("{}", ref.error());
            this->printer.printErr(LINGLONG_ERRV("Cannot find such application.",
                                                 utils::error::ErrorCode::AppNotFoundFromLocal));
            return -1;
        }

        auto layer = (*repo)->getLayerDir(*ref, "binary");
        if (!layer) {
            this->printer.printErr(layer.error());
            return -1;
        }

        auto info = layer->info();
        if (!info) {
            this->printer.printErr(info.error());
            return -1;
        }
        this->printer.printPackage(*info);

        return 0;
    }

    // 如果是layer文件，显示layer文件 包信息
    const auto layerFile = package::LayerFile::New(app);

    if (!layerFile) {
        this->printer.printErr(layerFile.error());
        return -1;
    }

    const auto layerInfo = (*layerFile)->metaInfo();
    if (!layerInfo) {
        this->printer.printErr(layerInfo.error());
        return -1;
    }

    this->printer.printLayerInfo(*layerInfo);
    return 0;
}

int Cli::content(const ContentOptions &options)
{
    LINGLONG_TRACE("command content");

    QStringList contents{};

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto ref = (*repo)->clearReferenceLocal(*fuzzyRef);
    if (!ref) {
        LogD("{}", ref.error());
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    auto layerItem = (*repo)->getLayerItem(*ref);
    if (!layerItem) {
        this->printer.printErr(layerItem.error());
        return -1;
    }

    if (layerItem->info.kind != "app") {
        this->printer.printErr(LINGLONG_ERRV("Only supports viewing app content"));
        return -1;
    }

    auto layer = (*repo)->getLayerDir(*ref, "binary");
    if (!layer) {
        this->printer.printErr(layer.error());
        return -1;
    }

    QDir entriesDir((layer->path() / "entries").c_str());
    if (!entriesDir.exists()) {
        this->printer.printErr(LINGLONG_ERR("no entries found").value());
        return -1;
    }

    const auto preferLibSystemdUser = QFileInfo(entriesDir.filePath("lib/systemd/user")).exists();

    QDirIterator it(entriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDot | QDir::NoDotDot | QDir::System,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        it.next();
        const auto entryPath = it.fileInfo().absoluteFilePath();
        const auto relativePath =
          std::filesystem::path(entriesDir.relativeFilePath(entryPath).toStdString());
        const auto exportPath = (*repo)->resolveEntryExportPath(relativePath, preferLibSystemdUser);
        if (!exportPath.empty()) {
            contents.append(QString::fromStdString(exportPath.string()));
        }
    }

    // only show the contents which are exported
    QStringList exportedContents{};
    for (const auto &content : std::as_const(contents)) {
        QFileInfo info(content);
        if (!info.exists() || info.isDir()) {
            continue;
        }
        exportedContents.append(content);
    }

    this->printer.printContent(exportedContents);
    return 0;
}

[[nodiscard]] std::string Cli::mappingFile(const std::filesystem::path &file) noexcept
{
    std::error_code ec;
    auto target = file;

    if (!target.is_absolute()) {
        return target;
    }

    if (std::filesystem::is_symlink(file, ec)) {
        std::array<char, PATH_MAX + 1> buf{};
        auto *real = ::realpath(file.c_str(), buf.data());

        if (real != nullptr) {
            target = real;
        } else {
            LogE("resolve symlink {} error: {}", file, common::error::errorString(errno));
        }
    }

    if (ec) {
        LogE("failed to check symlink {}: {}", file.c_str(), ec.message());
    }

    auto *homePath = ::getenv("HOME");
    if (homePath == nullptr || homePath[0] == '\0') {
        LogE("failed to get HOME env");
        return target;
    }

    // Don't map files under the user's home directory
    auto homeStr = std::string(homePath);
    if (homeStr.back() != '/') {
        homeStr.push_back('/');
    }
    if (auto tmp = target.string(); tmp.rfind(homeStr, 0) == 0) {
        return target;
    }

    return std::filesystem::path{ "/run/host/rootfs" }
    / std::filesystem::path{ target }.lexically_relative("/");
}

[[nodiscard]] std::string Cli::mappingUrl(std::string_view url) noexcept
{
    if (url.rfind('/', 0) == 0) {
        return mappingFile(url);
    }

    // if the scheme of url is "file", we need to map the native file path to the corresponding
    // container path, others will deliver to app directly.
    constexpr std::string_view filePrefix = "file://";
    if (url.rfind(filePrefix, 0) == 0) {
        std::filesystem::path nativePath = url.substr(filePrefix.size());
        std::filesystem::path target = mappingFile(nativePath);
        return std::string{ filePrefix } + target.string();
    }

    return std::string{ url };
}

std::vector<std::string> Cli::filePathMapping(const std::vector<std::string> &command,
                                              const RunOptions &options) const noexcept
{
    // FIXME: couldn't handle command like 'll-cli run org.xxx.yyy --file f1 f2 f3 org.xxx.yyy %%F'
    // can't distinguish the boundary of command , need validate the command arguments in the future

    std::vector<std::string> execArgs;
    // if the --file or --url option is specified, need to map the file path to the linglong
    // path(/run/host).
    for (const auto &arg : command) {
        if (arg.rfind('%', 0) != 0) {
            execArgs.emplace_back(arg);
            continue;
        }

        if (arg == "%f" || arg == "%F") {
            if (arg == "%f" && options.filePaths.size() > 1) {
                // refer:
                // https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html
                LogW("more than one file path specified, all file paths will be passed.");
            }

            for (const auto &file : options.filePaths) {
                if (file.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingFile(file));
            }

            continue;
        }

        if (arg == "%u" || arg == "%U") {
            if (arg == "%u" && options.fileUrls.size() > 1) {
                LogW("more than one url specified, all file paths will be passed.");
            }

            for (const auto &url : options.fileUrls) {
                if (url.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingUrl(url));
            }

            continue;
        }

        LogW("unknown command argument {}", arg);
    }

    return execArgs;
}

void Cli::filterPackageInfosByType(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list,
  const std::string &type) noexcept
{
    // if type is all, do nothing, return app of all packages.
    if (type == "all") {
        return;
    }

    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> filtered;

    for (const auto &[key, packages] : list) {
        std::vector<api::types::v1::PackageInfoV2> filteredPackages;

        std::copy_if(packages.begin(),
                     packages.end(),
                     std::back_inserter(filteredPackages),
                     [&type](const api::types::v1::PackageInfoV2 &pkg) {
                         return pkg.kind == type;
                     });

        if (!filteredPackages.empty()) {
            filtered.emplace(key, std::move(filteredPackages));
        }
    }

    list = std::move(filtered);
}

void Cli::filterPackageInfosByType(std::vector<api::types::v1::PackageInfoDisplay> &list,
                                   const std::string &type)
{
    if (type == "all") {
        return;
    }
    list.erase(std::remove_if(list.begin(),
                              list.end(),
                              [&type](const api::types::v1::PackageInfoDisplay &pkg) {
                                  return pkg.kind != type;
                              }),
               list.end());
}

void Cli::filterPackageInfosByVersion(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list) noexcept
{
    for (const auto &[pkgRepo, packages] : list) {
        std::map<std::string, api::types::v1::PackageInfoV2> temp;
        for (const auto &pkgInfo : packages) {
            auto key =
              QString("%1-%2-%3")
                .arg(pkgRepo.c_str(), pkgInfo.id.c_str(), pkgInfo.packageInfoV2Module.c_str())
                .toStdString();

            auto it = temp.find(key);
            if (it == temp.end()) {
                temp.emplace(key, pkgInfo);
                continue;
            }

            auto oldVersion = package::Version::parse(it->second.version);
            if (!oldVersion) {
                LogW("failed to parse old version: {}", oldVersion.error());
                continue;
            }

            auto newVersion = package::Version::parse(pkgInfo.version);
            if (!newVersion) {
                LogW("failed to parse new version: {}", newVersion.error());
                continue;
            }

            if (*oldVersion < *newVersion) {
                it->second = pkgInfo;
            }
        }

        if (temp.empty()) {
            continue;
        }

        std::vector<api::types::v1::PackageInfoV2> filteredPackages;
        filteredPackages.reserve(temp.size());

        for (auto &[_, pkgInfo] : temp) {
            filteredPackages.emplace_back(std::move(pkgInfo));
        }

        auto it = list.find(pkgRepo);
        if (it != list.end()) {
            it->second = std::move(filteredPackages);
        } else {
            list.emplace(pkgRepo, std::move(filteredPackages));
        }
    }
}

utils::error::Result<std::filesystem::path> Cli::ensureCache(runtime::RunContext &context) noexcept
{
    LINGLONG_TRACE("ensure cache via PM");

    const auto &containerID = context.getContainerId();
    auto targetItem = context.getCachedTargetItem();
    if (!targetItem) {
        return LINGLONG_ERR("failed to get cached target item", targetItem);
    }

    auto appCache = common::dir::getContainerCacheDir(targetItem->commit, containerID);
    auto runContextConfigFile = appCache / ".config";
    std::error_code ec;
    if (std::filesystem::exists(runContextConfigFile, ec)) {
        return appCache;
    }
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to check {}", runContextConfigFile), ec);
    }

    std::optional<QString> pendingJobID;
    bool success = false;
    QEventLoop loop;
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    if (QObject::connect(*pkgMan,
                         &api::dbus::v1::PackageManager::InitRunContextFinished,
                         &loop,
                         [&success, &loop, &pendingJobID](const QString &taskID, bool taskSuccess) {
                             if (!pendingJobID || taskID != pendingJobID) {
                                 return;
                             }
                             success = taskSuccess;
                             loop.quit();
                         })
        == nullptr) {
        return LINGLONG_ERR("failed to connect InitRunContextFinished signal");
    }

    auto cfgJson = nlohmann::json(context.getConfig()).dump();
    auto reply = (*pkgMan)->InitRunContext(QString::fromStdString(cfgJson),
                                           QString::fromStdString(containerID));
    QDBusPendingReply<QVariantMap> pendingReply = reply;
    auto resultRet = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!resultRet) {
        return LINGLONG_ERR(resultRet);
    }
    pendingJobID = QString::fromStdString(resultRet->id);

    loop.exec();

    if (!success) {
        return LINGLONG_ERR("InitRunContext failed", utils::error::ErrorCode::Failed);
    }

    return appCache;
}

void Cli::updateAM() noexcept
{
    // NOTE: make sure AM refresh the cache of desktop
    if ((QSysInfo::productType() == "Deepin" || QSysInfo::productType() == "deepin")
        && this->taskState.state == linglong::api::types::v1::State::Succeed) {
        QDBusConnection conn = QDBusConnection::systemBus();
        if (!conn.isConnected()) {
            LogW("Failed to connect to the system bus");
        }

        auto peer = linglong::api::dbus::v1::DBusPeer("org.desktopspec.ApplicationUpdateNotifier1",
                                                      "/org/desktopspec/ApplicationUpdateNotifier1",
                                                      conn);
        auto reply = peer.Ping();
        reply.waitForFinished();
        if (!reply.isValid()) {
            LogW("Failed to ping org.desktopspec.ApplicationUpdateNotifier1: {}",
                 reply.error().message().toStdString());
        }
    }
}

int Cli::inspect(CLI::App *app, const InspectOptions &options)
{
    LINGLONG_TRACE("command inspect");

    auto argsParseFunc = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParseFunc("dir")) {
        if (options.dirType == "layer") {
            return this->getLayerDir(options);
        } else if (options.dirType == "bundle") {
            return this->getBundleDir(options);
        } else {
            this->printer.printErr(LINGLONG_ERRV(
              fmt::format("Invalid type: {}, type must be layer or bundle", options.dirType)));
            return -1;
        }
    }

    return 0;
}

int Cli::getLayerDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Layer dir");

    auto fuzzyString = options.appid;

    auto fuzzyRef = package::FuzzyReference::parse(fuzzyString);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto repo = this->getRepo();
    if (!repo) {
        this->printer.printErr(repo.error());
        return -1;
    }

    auto ref = (*repo)->clearReferenceLocal(*fuzzyRef);
    if (!ref) {
        LogD("{}", ref.error());
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    std::string module = "binary";
    if (!options.module.empty()) {
        module = options.module;
    }

    auto layerDir = (*repo)->getLayerDir(*ref, module);
    if (!layerDir) {
        this->printer.printErr(layerDir.error());
        return -1;
    }

    std::cout << layerDir->path().string() << std::endl;

    return 0;
}

int Cli::getBundleDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Bundle dir");

    auto containerIDList = getRunningAppContainers(options.appid);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("Can not find the running application."));
        return -1;
    }

    auto bundleDir = linglong::common::dir::getBundleDir(containerIDList->front());

    std::cout << bundleDir.string() << std::endl;

    return 0;
}

utils::error::Result<void> Cli::syncTaskProperties()
{
    LINGLONG_TRACE("syncTaskProperties");

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    QDBusInterface properties((*pkgMan)->service(),
                              this->taskObjectPath,
                              "org.freedesktop.DBus.Properties",
                              (*pkgMan)->connection());
    QDBusReply<QVariantMap> reply =
      properties.call("GetAll", QStringLiteral("org.deepin.linglong.Task1"));
    if (!reply.isValid()) {
        if (reply.error().type() == QDBusError::UnknownObject) {
            return LINGLONG_OK;
        }

        return LINGLONG_ERR(reply.error().message().toStdString());
    }

    this->onTaskPropertiesChanged(QStringLiteral("org.deepin.linglong.Task1"), reply.value(), {});

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::waitTaskCreated(QDBusPendingReply<QVariantMap> &reply,
                                                TaskType taskType)
{
    LINGLONG_TRACE("waitTaskCreated");

    auto result = waitDBusReply<api::types::v1::PackageManager1PackageTaskResult>(reply);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
    if (resultCode != utils::error::ErrorCode::Success) {
        return LINGLONG_ERR(result->message, result->code);
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    auto conn = (*pkgMan)->connection();
    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    this->task = new api::dbus::v1::Task1((*pkgMan)->service(), taskObjectPath, conn);
    this->taskState.state = linglong::api::types::v1::State::Queued;
    this->taskState.taskType = taskType;

    LogD("task object path: {}", this->taskObjectPath.toStdString());

    if (!conn.connect((*pkgMan)->service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        Q_ASSERT(false);
        return LINGLONG_ERR(fmt::format("Failed to connect signal PropertiesChanged: {}",
                                        conn.lastError().message().toStdString()));
    }

    return this->syncTaskProperties();
}

void Cli::waitTaskDone()
{
    if (this->taskState.state == api::types::v1::State::Failed
        || this->taskState.state == api::types::v1::State::Canceled
        || this->taskState.state == api::types::v1::State::Succeed) {
        return;
    }

    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        LogE("connect taskDone failed");
        return;
    }
    loop.exec();
}

void Cli::handleInstallError(const utils::error::Error &error,
                             const api::types::v1::PackageManager1InstallParameters &params)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppInstallModuleRequireAppFirst:
        this->printer.printMessage(_("To install the module, one must first install the app."));
        break;
    case utils::error::ErrorCode::AppInstallModuleAlreadyExists:
        this->printer.printMessage(_("Module is already installed."));
        break;
    case utils::error::ErrorCode::AppInstallModuleNotFound:
        this->printer.printMessage(_("The module could not be found remotely."));
        break;
    case utils::error::ErrorCode::AppInstallAlreadyInstalled:
        this->printer.printMessage(_("Application already installed"));
        break;
    case utils::error::ErrorCode::AppInstallNotFoundFromRemote:
        this->printer.printMessage(
          fmt::format(_("Application {} is not found in remote repo."), params.package.id));
        break;
    case utils::error::ErrorCode::AppInstallModuleNoVersion:
        this->printer.printMessage(_("Cannot specify a version when installing a module."));
        break;
    case utils::error::ErrorCode::AppInstallNeedDowngrade:
        this->printer.printMessage(
          fmt::format(_("The latest version has been installed. If you want to "
                        "replace it, try using 'll-cli install {} --force'"),
                      params.package.id));
        break;
    case utils::error::ErrorCode::Unknown:
    case utils::error::ErrorCode::AppInstallFailed:
        this->printer.printMessage(_("Install failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleInstallFromFileError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppInstallAlreadyInstalled:
        this->printer.printMessage(_("Application already installed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUninstallError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUninstallAppIsRunning: {

        auto ret = this->notifier->notify(api::types::v1::InteractionRequest{
          .appName = "ll-cli",
          .summary = _("The application is currently running and cannot be "
                       "uninstalled. Please turn off the application and try again.") });
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        break;
    }
    case utils::error::ErrorCode::AppUninstallNotFoundFromLocal:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUninstallMultipleVersions:
        this->printer.printMessage(
          fmt::format(_("Multiple versions of the package are installed. Please specify a single "
                        "version to uninstall:\n{}"),
                      error.message()));
        break;
    case utils::error::ErrorCode::AppUninstallBaseOrRuntime:
        this->printer.printMessage(
          _("Base or runtime cannot be uninstalled, please use 'll-cli prune'."));
        break;
    case utils::error::ErrorCode::AppUninstallFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Uninstall failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUpgradeError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUpgradeLocalNotFound:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUpgradeFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Upgrade failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

bool Cli::handleCommonError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::NetworkError:
        this->printer.printMessage(_("Network connection failed. Please:"
                                     "\n1. Check your internet connection"
                                     "\n2. Verify network proxy settings if used"));
        break;
    case utils::error::ErrorCode::LayerCompatibilityError:
        this->printer.printMessage(_("Package not found"));
        break;
    case utils::error::ErrorCode::Canceled:
        this->printer.printMessage(_("Operation canceled"));
        break;
    case utils::error::ErrorCode::PermissionDenied:
        this->printer.printMessage(_("Permission denied, authentication is required"));
        break;
    default:
        this->printer.printErr(error);
        return false;
    }

    return true;
}

void Cli::detectDrivers()
{
    QProcess process;
    process.setProgram(QString(LINGLONG_LIBEXEC_DIR "/ll-driver-detect"));
    // 禁用标准输入 (stdin)
    process.setStandardInputFile("/dev/null");
    // 禁用标准输出 (stdout)
    process.setStandardOutputFile("/dev/null");
    // 禁用标准错误输出 (stderr)
    process.setStandardErrorFile("/dev/null");
    process.startDetached();
}

} // namespace linglong::cli
