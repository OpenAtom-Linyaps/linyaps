/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/scoped_runtime_paths.h"
#include "../../common/tempdir.h"
#include "linglong/adaptors/package_manager/package_manager1.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/serialize/json.h"
#include "linglong/package/architecture.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

using namespace linglong;
namespace v1 = api::types::v1;

constexpr const char *kServiceName = "org.deepin.linglong.PackageManagerCommandTest";
constexpr const char *kObjectPath = "/org/deepin/linglong/PackageManager1";
constexpr const char *kInterfaceName = "org.deepin.linglong.PackageManager1";

v1::RepoConfigV2 makeConfig()
{
    return v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { v1::Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };
}

v1::RepositoryCacheLayersItem makeLayerItem(const std::string &version,
                                            const std::string &kind,
                                            const std::string &module)
{
    return v1::RepositoryCacheLayersItem{
        .commit = "commit-" + version,
        .deleted = std::nullopt,
        .info =
          v1::PackageInfoV2{
            .arch = std::vector<std::string>{ "x86_64" },
            .channel = "main",
            .id = "org.example.App",
            .kind = kind,
            .packageInfoV2Module = module,
            .version = version,
          },
        .repo = "stable",
    };
}

long long replyCode(const QVariantMap &result)
{
    return result.value("code").toLongLong();
}

QString replyMessage(const QVariantMap &result)
{
    return result.value("message").toString();
}

class TaskResultReceiver : public QObject
{
    Q_OBJECT

public:
    QVariantMap result;
    bool finished{ false };

public Q_SLOTS:

    void onTaskFinished(const QVariantMap &payload)
    {
        result = payload;
        finished = true;
    }
};

class PrivateBus
{
public:
    bool start()
    {
        auto *pipe = ::popen("dbus-daemon --session --fork --print-address=1 --print-pid=1", "r");
        if (pipe == nullptr) {
            return false;
        }

        char addressBuf[1024] = {};
        char pidText[64] = {};
        const bool readAddress = std::fgets(addressBuf, sizeof(addressBuf), pipe) != nullptr;
        const bool readPid = std::fgets(pidText, sizeof(pidText), pipe) != nullptr;
        ::pclose(pipe);

        if (!readAddress || !readPid) {
            return false;
        }

        std::string addr{ addressBuf };
        while (!addr.empty() && (addr.back() == '\n' || addr.back() == '\r')) {
            addr.pop_back();
        }
        if (addr.empty()) {
            return false;
        }

        pid = static_cast<pid_t>(std::strtol(pidText, nullptr, 10));
        if (pid <= 0) {
            return false;
        }

        address = addr;
        return true;
    }

    void stop()
    {
        if (pid > 0) {
            ::kill(pid, SIGTERM);
            int status = 0;
            ::waitpid(pid, &status, 0);
            pid = -1;
        }
    }

    ~PrivateBus() { stop(); }

    const std::string &busAddress() const { return address; }

private:
    pid_t pid{ -1 };
    std::string address;
};

struct SharedBus
{
    PrivateBus daemon;
    bool ready{ false };

    SharedBus() { ready = daemon.start(); }
};

SharedBus &sharedBus()
{
    static SharedBus shared;
    return shared;
}

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path, v1::RepoConfigV2 config)
        : repo::OSTreeRepo(path, std::move(config))
    {
    }

    bool initialize() { return this->init(true).has_value(); }

    std::function<utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>(
      const repo::repoCacheQuery &)>
      onListLocalBy;
    std::function<utils::error::Result<std::vector<v1::PackageInfoV2>>(std::string,
                                                                       const v1::Repo &)>
      onSearchRemote;

    void useRealQueries()
    {
        onListLocalBy = [this](const repo::repoCacheQuery &query) {
            return repo::OSTreeRepo::listLocalBy(query);
        };
    }

    utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>
    listLocalBy(const repo::repoCacheQuery &query) const noexcept override
    {
        LINGLONG_TRACE("listLocalBy");
        if (onListLocalBy) {
            return onListLocalBy(query);
        }
        return LINGLONG_ERR("not mocked");
    }

    utils::error::Result<std::vector<v1::PackageInfoV2>>
    searchRemote(std::string id, const v1::Repo &repo) const noexcept override
    {
        LINGLONG_TRACE("searchRemote");
        if (onSearchRemote) {
            return onSearchRemote(id, repo);
        }
        return std::vector<v1::PackageInfoV2>{};
    }
};

std::string hostArch()
{
    return package::Architecture::currentCPUArchitecture().toString();
}

std::string foreignArch()
{
    return hostArch() == "arm64" ? "x86_64" : "arm64";
}

v1::PackageInfoV2 makePackageInfo(const std::string &id,
                                  const std::string &version,
                                  const std::string &kind,
                                  const std::string &module,
                                  const std::string &arch = hostArch())
{
    return v1::PackageInfoV2{
        .arch = std::vector<std::string>{ arch },
        .channel = "main",
        .id = id,
        .kind = kind,
        .packageInfoV2Module = module,
        .version = version,
    };
}

QVariantMap makeCommonOptions(bool force = false, bool skipInteraction = false)
{
    QVariantMap options;
    options.insert("force", force);
    options.insert("skipInteraction", skipInteraction);
    return options;
}

struct PackedLayer
{
    std::filesystem::path dir;
    std::filesystem::path file;
};

std::optional<PackedLayer> packLayer(const std::filesystem::path &root,
                                     const std::string &name,
                                     const v1::PackageInfoV2 &info)
{
    const auto dir = root / ("layer-" + name);
    std::error_code ec;
    std::filesystem::create_directories(dir / "files", ec);
    if (ec) {
        ADD_FAILURE() << "failed to create layer directory: " << ec.message();
        return std::nullopt;
    }
    std::ofstream(dir / "files" / "payload") << "payload";
    std::ofstream(dir / "info.json") << nlohmann::json(info).dump();

    const auto file = root / (name + ".layer");
    package::LayerPackager packager;
    auto packed = packager.pack(package::LayerDir(dir), QString::fromStdString(file.string()));
    if (!packed) {
        ADD_FAILURE() << "failed to pack layer: " << packed.error().message();
        return std::nullopt;
    }
    return PackedLayer{ dir, file };
}

constexpr auto kFuseMountProbeTimeout = std::chrono::seconds(5);
constexpr auto kFuseMountProbeInterval = std::chrono::milliseconds(50);

enum class FuseMountProbeResult {
    Available,
    Unavailable,
    Failed,
};

bool looksLikePermissionError(const std::string &message)
{
    return message.find("EACCES") != std::string::npos || message.find("EPERM") != std::string::npos
      || message.find("Permission denied") != std::string::npos
      || message.find("permission denied") != std::string::npos
      || message.find("Operation not permitted") != std::string::npos;
}

TEST(FuseMountProbeTest, ClassifiesPermissionFailuresAsEnvironment)
{
    struct TestCase
    {
        const char *message;
        bool permissionError;
    };

    const TestCase cases[] = {
        { "fusermount3: mount failed: Permission denied", true },
        { "mount failed: EACCES", true },
        { "mount failed: EPERM", true },
        { "mount failed: Operation not permitted", true },
        { "fusermount3: mount failed: No such file or directory", false },
        { "failed to read erofs super block", false },
        { "", false },
    };

    for (const auto &tc : cases) {
        SCOPED_TRACE(tc.message);
        EXPECT_EQ(looksLikePermissionError(tc.message), tc.permissionError);
    }
}

// FUSE mounts are not permitted in every environment (for example the GitHub
// Actions runner returns EACCES). Probe with a real mount of an erofs image
// built independently of the layer under test, so a broken layer product or
// parser can never be reported as an environment skip.
//
// The probe must mount under the same work-directory tree as the real layer
// task. Ubuntu 26.04 ships an AppArmor profile for fusermount3 that allows
// mounts under /tmp but not under /var/tmp; probing in /tmp therefore reports
// Available while LayerPackager's default /var/tmp mount is denied.
FuseMountProbeResult probeFuseMount(std::string &reason)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        reason = "fuse device is not available";
        return FuseMountProbeResult::Unavailable;
    }

    const int fuseFd = ::open("/dev/fuse", O_RDWR | O_CLOEXEC);
    if (fuseFd < 0) {
        reason =
          "cannot access /dev/fuse: " + std::error_code(errno, std::generic_category()).message();
        return FuseMountProbeResult::Unavailable;
    }
    ::close(fuseFd);

    if (!utils::Cmd("mkfs.erofs").exists()) {
        reason = "mkfs.erofs is not available";
        return FuseMountProbeResult::Unavailable;
    }

    if (!utils::Cmd("erofsfuse").exists()) {
        reason = "erofsfuse is not available";
        return FuseMountProbeResult::Unavailable;
    }

    TempDir fixtureRoot("linglong-fuse-fixture-");
    const auto fixtureDir = fixtureRoot.path() / "root";
    std::filesystem::create_directories(fixtureDir);
    std::ofstream(fixtureDir / "info.json") << "{}";
    const auto fixtureImage = fixtureRoot.path() / "fixture.erofs";
    auto imageBuilt =
      utils::Cmd("mkfs.erofs").exec({ "-b4096", fixtureImage.string(), fixtureDir.string() });
    if (!imageBuilt) {
        reason = "failed to build a known-good erofs probe image: " + imageBuilt.error().message();
        return FuseMountProbeResult::Failed;
    }

    package::LayerPackager probeLayout;
    const auto mountBase = probeLayout.getWorkDir();
    if (mountBase.empty()) {
        reason = "cannot determine the layer work directory for the fuse probe";
        return FuseMountProbeResult::Unavailable;
    }

    const auto mountPoint = mountBase / "unpack";
    std::error_code ec;
    std::filesystem::create_directories(mountPoint, ec);
    if (ec) {
        reason = "cannot create the fuse probe mount point under " + mountBase.string() + ": "
          + ec.message();
        return FuseMountProbeResult::Unavailable;
    }
    auto cleanupMountPoint = utils::finally::finally([&mountPoint] {
        std::error_code cleanupEc;
        std::filesystem::remove_all(mountPoint, cleanupEc);
    });

    auto mounted =
      utils::Cmd("erofsfuse").exec({ "--offset=0", fixtureImage.string(), mountPoint.string() });

    // erofsfuse daemonizes and can exit successfully even when the mount itself
    // fails, so wait until the known payload can actually be read back.
    const auto probeFile = mountPoint / "info.json";
    bool mountReady = false;
    bool payloadMismatch = false;
    auto readProbePayload = [&probeFile, &payloadMismatch] {
        std::ifstream input(probeFile);
        if (!input) {
            return false;
        }
        std::string payload;
        if (!std::getline(input, payload)) {
            return false;
        }
        if (payload != "{}") {
            payloadMismatch = true;
            return false;
        }
        return true;
    };
    if (mounted) {
        mountReady = readProbePayload();
        for (auto waited = std::chrono::milliseconds(0);
             !mountReady && !payloadMismatch && waited < kFuseMountProbeTimeout;
             waited += kFuseMountProbeInterval) {
            std::this_thread::sleep_for(kFuseMountProbeInterval);
            mountReady = readProbePayload();
        }
    }

    std::string unmountDiagnostics;
    for (const auto *tool : { "fusermount3", "fusermount" }) {
        utils::Cmd unmount(tool);
        if (!unmount.exists()) {
            unmountDiagnostics += std::string(tool) + " is not available; ";
            continue;
        }

        auto unmounted = unmount.exec({ "-z", "-u", mountPoint.string() });
        if (unmounted) {
            unmountDiagnostics.clear();
            break;
        }
        unmountDiagnostics += std::string(tool) + " failed: " + unmounted.error().message() + "; ";
    }

    if (!mounted) {
        reason = "erofsfuse failed to mount a known-good erofs image: " + mounted.error().message();
        if (!unmountDiagnostics.empty()) {
            reason += " (cleanup: " + unmountDiagnostics + ")";
        }
        if (looksLikePermissionError(mounted.error().message())) {
            return FuseMountProbeResult::Unavailable;
        }
        return FuseMountProbeResult::Failed;
    }

    if (payloadMismatch) {
        reason = "the known-good erofs probe mounted under " + mountBase.string()
          + " but its payload could not be read back";
        return FuseMountProbeResult::Failed;
    }

    if (!mountReady) {
        reason = "a known-good erofs image did not become readable under " + mountBase.string()
          + ", assuming fuse mounts are not permitted there";
        if (!unmountDiagnostics.empty()) {
            reason += " (unmount: " + unmountDiagnostics + ")";
        }
        return FuseMountProbeResult::Unavailable;
    }

    if (!unmountDiagnostics.empty()) {
        std::cerr << "[  WARNING ] failed to unmount the fuse probe mount point " << mountPoint
                  << ": " << unmountDiagnostics << std::endl;
    }

    return FuseMountProbeResult::Available;
}

class PackageManagerCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto &shared = sharedBus();
        if (!shared.ready) {
            GTEST_SKIP() << "a private dbus-daemon is required for package manager command tests";
        }

        if (QCoreApplication::instance() == nullptr) {
            static int argc = 0;
            application = std::make_unique<QCoreApplication>(argc, nullptr);
        }

        connectionName = QString("linglong-test-%1").arg(instanceCounter++);
        connection =
          QDBusConnection::connectToBus(QString::fromStdString(shared.daemon.busAddress()),
                                        connectionName);
        if (!connection.isConnected()) {
            GTEST_SKIP() << "failed to connect to the private session bus";
        }
        serviceName = QString("%1.T%2").arg(kServiceName).arg(instanceCounter++);

        tempDir = std::make_unique<TempDir>();
        repoDir = tempDir->path() / "repo";
        std::filesystem::create_directories(repoDir);

        config = makeConfig();
        repoOwner = std::make_unique<MockRepo>(repoDir, config);
        ASSERT_TRUE(repoOwner->initialize());
        repo = repoOwner.get();

        pmOwner =
          std::make_unique<service::PackageManager>(std::move(repoOwner),
                                                    std::unique_ptr<runtime::ContainerBuilder>(),
                                                    nullptr);
        pm = pmOwner.get();
        pm->initDaemonMode(peerMode());
        adaptor = new linglong::adaptors::package_manger::PackageManager1(pm);

        if (!connection.registerService(serviceName)) {
            ADD_FAILURE() << "registerService failed for " << serviceName.toStdString() << ": "
                          << connection.lastError().name().toStdString() << " "
                          << connection.lastError().message().toStdString();
        }
        ASSERT_TRUE(connection.registerObject(kObjectPath, pm));
    }

    void TearDown() override
    {
        if (connection.isConnected()) {
            connection.unregisterObject(kObjectPath);
            connection.unregisterService(serviceName);
        }
        pmOwner.reset();
        adaptor = nullptr;
        repo = nullptr;
        repoOwner.reset();
        tempDir.reset();
        QDBusConnection::disconnectFromBus(connectionName);
        connection = QDBusConnection(QString());
        // Qt only supports one QCoreApplication per process; keep it alive.
        application.release();
    }

    virtual bool peerMode() const { return true; }

    QVariantMap callCommand(const QString &method, const QVariantMap &params)
    {
        QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
        auto reply = iface.call(method, params);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            ADD_FAILURE() << method.toStdString()
                          << " failed: " << reply.errorMessage().toStdString();
            return {};
        }
        const auto argument = reply.arguments().value(0);
        if (argument.canConvert<QDBusArgument>()) {
            return qdbus_cast<QVariantMap>(argument.value<QDBusArgument>());
        }
        return argument.toMap();
    }

    QVariantMap callCommandWithoutArguments(const QString &method)
    {
        QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
        auto reply = iface.call(method);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            ADD_FAILURE() << method.toStdString()
                          << " failed: " << reply.errorMessage().toStdString();
            return {};
        }
        const auto argument = reply.arguments().value(0);
        if (argument.canConvert<QDBusArgument>()) {
            return qdbus_cast<QVariantMap>(argument.value<QDBusArgument>());
        }
        return argument.toMap();
    }

    QVariantMap callInstallFromFile(const QDBusUnixFileDescriptor &fd,
                                    const QString &fileType,
                                    const QVariantMap &options)
    {
        QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
        auto reply = iface.call("InstallFromFile", QVariant::fromValue(fd), fileType, options);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            ADD_FAILURE() << "InstallFromFile failed: " << reply.errorMessage().toStdString();
            return {};
        }
        const auto argument = reply.arguments().value(0);
        if (argument.canConvert<QDBusArgument>()) {
            return qdbus_cast<QVariantMap>(argument.value<QDBusArgument>());
        }
        return argument.toMap();
    }

    static bool waitFor(const std::atomic_bool &flag,
                        std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!flag.load() && std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return flag.load();
    }

    bool startTaskAndWait(const QString &taskPath, TaskResultReceiver &receiver)
    {
        if (!connection.connect(serviceName,
                                taskPath,
                                "org.deepin.linglong.Task1",
                                "TaskFinished",
                                &receiver,
                                SLOT(onTaskFinished(QVariantMap)))) {
            return false;
        }

        QDBusInterface taskIface(serviceName, taskPath, "org.deepin.linglong.Task1", connection);
        if (!taskIface.isValid()) {
            return false;
        }
        if (taskIface.call("Start").type() == QDBusMessage::ErrorMessage) {
            return false;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!receiver.finished && std::chrono::steady_clock::now() < deadline) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return receiver.finished;
    }

    static std::atomic<int> instanceCounter;
    std::unique_ptr<QCoreApplication> application;
    QString connectionName;
    QString serviceName;
    QDBusConnection connection{ QString() };
    std::unique_ptr<TempDir> tempDir;
    std::filesystem::path repoDir;
    v1::RepoConfigV2 config;
    std::unique_ptr<MockRepo> repoOwner;
    MockRepo *repo{ nullptr };
    std::unique_ptr<service::PackageManager> pmOwner;
    linglong::adaptors::package_manger::PackageManager1 *adaptor{ nullptr };
    service::PackageManager *pm{ nullptr };
};

std::atomic<int> PackageManagerCommandTest::instanceCounter{ 0 };

TEST_F(PackageManagerCommandTest, InstallInvalidParametersReturnsInstallFailed)
{
    auto result = callCommand("Install", {});
    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppInstallFailed));
    EXPECT_FALSE(replyMessage(result).isEmpty());
}

TEST_F(PackageManagerCommandTest, InstallUnknownRepoAliasReturnsInstallFailed)
{
    auto params = v1::PackageManager1InstallParameters{
        .options =
          v1::CommonOptions{ .force = false, .noAutoPrune = false, .skipInteraction = true },
        .package =
          v1::PackageManager1InstallParametersPacakge{
            .channel = "main",
            .id = "org.example.App",
            .version = "1.0.0",
          },
        .repo = "missing",
    };

    auto result = callCommand("Install", common::serialize::toQVariantMap(params));
    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppInstallFailed));
    EXPECT_THAT(replyMessage(result).toStdString(), ::testing::HasSubstr("no repo found"));
}

TEST_F(PackageManagerCommandTest, InstallRejectsMalformedParameters)
{
    auto result = callCommand("Install",
                              QVariantMap{ { "package", QString("not-a-package") },
                                           { "options", QString("not-options") } });
    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppInstallFailed));
}

TEST_F(PackageManagerCommandTest, UninstallInvalidParametersReturnsUninstallFailed)
{
    auto result = callCommand("Uninstall", {});
    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallFailed));
}

TEST_F(PackageManagerCommandTest, UninstallReportsLocalQueryFailure)
{
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>(
          tl::make_unexpected(utils::error::Error::Err(__FILE__, __LINE__, "", "query failed")));
    };

    auto params = v1::PackageManager1UninstallParameters{
        .options =
          v1::CommonOptions{ .force = false, .noAutoPrune = false, .skipInteraction = true },
        .package = v1::PackageManager1Package{ .channel = "main",
                                               .id = "org.example.App",
                                               .packageManager1PackageModule = "binary",
                                               .version = "1.0.0" },
    };

    auto result = callCommand("Uninstall", common::serialize::toQVariantMap(params));
    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallFailed));
    EXPECT_THAT(replyMessage(result).toStdString(), ::testing::HasSubstr("query failed"));
}

TEST_F(PackageManagerCommandTest, UninstallReportsMissingPackage)
{
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return std::vector<v1::RepositoryCacheLayersItem>{};
    };

    auto params = v1::PackageManager1UninstallParameters{
        .options =
          v1::CommonOptions{ .force = false, .noAutoPrune = false, .skipInteraction = true },
        .package = v1::PackageManager1Package{ .channel = "main",
                                               .id = "org.example.App",
                                               .packageManager1PackageModule = "binary",
                                               .version = "1.0.0" },
    };

    auto result = callCommand("Uninstall", common::serialize::toQVariantMap(params));
    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallNotFoundFromLocal));
}

TEST_F(PackageManagerCommandTest, UninstallRefusesBaseAndRuntimeWithoutForce)
{
    for (const auto &kind : { std::string("base"), std::string("runtime") }) {
        SCOPED_TRACE(kind);
        repo->onListLocalBy = [kind](const repo::repoCacheQuery &) {
            return std::vector<v1::RepositoryCacheLayersItem>{
                makeLayerItem("1.0.0", kind, "binary")
            };
        };

        auto params = v1::PackageManager1UninstallParameters{
            .options =
              v1::CommonOptions{ .force = false, .noAutoPrune = false, .skipInteraction = true },
            .package = v1::PackageManager1Package{ .channel = "main",
                                                   .id = "org.example.App",
                                                   .packageManager1PackageModule = "binary",
                                                   .version = "1.0.0" },
        };

        auto result = callCommand("Uninstall", common::serialize::toQVariantMap(params));
        EXPECT_EQ(replyCode(result),
                  static_cast<long long>(utils::error::ErrorCode::AppUninstallBaseOrRuntime));
    }
}

TEST_F(PackageManagerCommandTest, UninstallReportsMultipleInstalledVersions)
{
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return std::vector<v1::RepositoryCacheLayersItem>{
            makeLayerItem("1.0.0", "app", "binary"),
            makeLayerItem("2.0.0", "app", "binary"),
        };
    };

    auto params = v1::PackageManager1UninstallParameters{
        .options =
          v1::CommonOptions{ .force = false, .noAutoPrune = false, .skipInteraction = true },
        .package = v1::PackageManager1Package{ .channel = "main",
                                               .id = "org.example.App",
                                               .packageManager1PackageModule = "binary",
                                               .version = "1.0.0" },
    };

    auto result = callCommand("Uninstall", common::serialize::toQVariantMap(params));
    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallMultipleVersions));
    const auto message = replyMessage(result).toStdString();
    EXPECT_THAT(message, ::testing::HasSubstr("1.0.0"));
    EXPECT_THAT(message, ::testing::HasSubstr("2.0.0"));
}

TEST_F(PackageManagerCommandTest, UpdateInvalidParametersReturnsUpgradeFailed)
{
    auto result = callCommand("Update", {});
    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppUpgradeFailed));
}

TEST_F(PackageManagerCommandTest, SearchInvalidParametersReturnsError)
{
    auto result = callCommand("Search", {});
    EXPECT_NE(replyCode(result), 0LL);
}

TEST_F(PackageManagerCommandTest, SearchQueuesTaskAndQueriesConfiguredRepo)
{
    std::atomic_bool searched{ false };
    repo->onSearchRemote = [&searched](std::string id, const v1::Repo &) {
        EXPECT_EQ(id, "org.example.App");
        searched = true;
        return std::vector<v1::PackageInfoV2>{};
    };

    auto result = callCommand(
      "Search",
      common::serialize::toQVariantMap(
        v1::PackageManager1SearchParameters{ .id = "org.example.App", .repos = { "stable" } }));

    EXPECT_EQ(replyCode(result), 0LL);
    EXPECT_THAT(result.value("message").toString().toStdString(),
                ::testing::HasSubstr("org.example.App"));
    EXPECT_TRUE(waitFor(searched));
}

TEST_F(PackageManagerCommandTest, InitRunContextRejectsMalformedConfig)
{
    auto result = pm->initRunContext("not-a-json", "container-id");
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("failed to parse run context config"));
}

TEST_F(PackageManagerCommandTest, InitRunContextRejectsMismatchedContainerId)
{
    auto result = pm->initRunContext(R"({"id":"org.example.App"})", "container-id");
    ASSERT_FALSE(result);
    EXPECT_FALSE(result.error().message().empty());
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsUnsupportedFileType)
{
    int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    QVariantMap options;
    options.insert("force", false);
    options.insert("skipInteraction", false);
    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "bogus", options);

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppInstallUnsupportedFileFormat));
    EXPECT_THAT(replyMessage(result).toStdString(),
                ::testing::HasSubstr("bogus is unsupported fileType"));
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsMalformedOptions)
{
    int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    QVariantMap options;
    options.insert("force", QVariantMap{});
    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "uab", options);

    EXPECT_NE(replyCode(result), 0LL);
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsBrokenLayerPayload)
{
    const auto payload = tempDir->path() / "broken.layer";
    std::ofstream{ payload } << "this is not an erofs layer";
    int fd = ::open(payload.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions());

    EXPECT_NE(replyCode(result), 0LL);
}

TEST_F(PackageManagerCommandTest, InstallFromFileQueuesValidLayer)
{
    auto layer = packLayer(tempDir->path(),
                           "valid",
                           makePackageInfo("org.example.Layer", "1.0.0.0", "app", "binary"));
    ASSERT_TRUE(layer);

    int fd = ::open(layer->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions());
    EXPECT_EQ(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(),
                ::testing::HasSubstr("layer file is now installing"));

    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsForeignArchitectureLayer)
{
    auto layer =
      packLayer(tempDir->path(),
                "foreign",
                makePackageInfo("org.example.Foreign", "1.0.0.0", "app", "binary", foreignArch()));
    ASSERT_TRUE(layer);

    int fd = ::open(layer->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions());
    EXPECT_NE(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(),
                ::testing::HasSubstr("not match host architecture"));
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsAlreadyInstalledLayer)
{
    auto layer = packLayer(tempDir->path(),
                           "already",
                           makePackageInfo("org.example.Already", "1.0.0.0", "app", "binary"));
    ASSERT_TRUE(layer);
    auto imported = repo->importLayerDir(package::LayerDir(layer->dir));
    ASSERT_TRUE(imported) << imported.error().message();

    int fd = ::open(layer->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions());
    EXPECT_NE(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(), ::testing::HasSubstr("already installed"));
}

TEST_F(PackageManagerCommandTest, InstallFromFileRejectsDowngradeWithoutForce)
{
    auto newer = packLayer(tempDir->path(),
                           "newer",
                           makePackageInfo("org.example.Downgrade", "2.0.0.0", "app", "binary"));
    ASSERT_TRUE(newer);
    auto imported = repo->importLayerDir(package::LayerDir(newer->dir));
    ASSERT_TRUE(imported) << imported.error().message();

    auto older = packLayer(tempDir->path(),
                           "older",
                           makePackageInfo("org.example.Downgrade", "1.0.0.0", "app", "binary"));
    ASSERT_TRUE(older);

    int fd = ::open(older->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result =
      callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions(false));
    EXPECT_NE(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(),
                ::testing::HasSubstr("latest version has been installed"));
}

TEST_F(PackageManagerCommandTest, InstallFromFileQueuesUpgradeLayerWithoutInteraction)
{
    auto older = packLayer(tempDir->path(),
                           "up-old",
                           makePackageInfo("org.example.Upgrade", "1.0.0.0", "app", "binary"));
    ASSERT_TRUE(older);
    auto imported = repo->importLayerDir(package::LayerDir(older->dir));
    ASSERT_TRUE(imported) << imported.error().message();

    auto newer = packLayer(tempDir->path(),
                           "up-new",
                           makePackageInfo("org.example.Upgrade", "2.0.0.0", "app", "binary"));
    ASSERT_TRUE(newer);

    int fd = ::open(newer->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result =
      callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions(false, true));
    EXPECT_EQ(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(),
                ::testing::HasSubstr("layer file is now installing"));

    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(PackageManagerCommandTest, InstallFromFileRunsQueuedRuntimeLayerTask)
{
    auto layer =
      packLayer(tempDir->path(),
                "runtime-task",
                makePackageInfo("org.example.RuntimeTask", "1.0.0.0", "runtime", "runtime"));
    ASSERT_TRUE(layer);

    auto layerFile = package::LayerFile::New(layer->file.string().c_str());
    ASSERT_TRUE(layerFile) << "the layer produced by packLayer is not a valid layer archive: "
                           << layerFile.error().message();
    auto layerOffset = (*layerFile)->binaryDataOffset();
    ASSERT_TRUE(layerOffset) << "failed to parse the layer produced by packLayer: "
                             << layerOffset.error().message();
    EXPECT_GT(*layerOffset, static_cast<quint32>(package::magicNumber().size()));

    std::string fuseSkipReason;
    switch (probeFuseMount(fuseSkipReason)) {
    case FuseMountProbeResult::Available:
        break;
    case FuseMountProbeResult::Unavailable:
        GTEST_SKIP() << fuseSkipReason;
        break;
    case FuseMountProbeResult::Failed:
        FAIL() << fuseSkipReason;
        break;
    }

    int fd = ::open(layer->file.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = callInstallFromFile(QDBusUnixFileDescriptor{ fd }, "layer", makeCommonOptions());
    ASSERT_EQ(replyCode(result), 0LL);
    const auto taskPath = result.value("taskObjectPath").toString();
    ASSERT_FALSE(taskPath.isEmpty());

    repo->useRealQueries();
    TaskResultReceiver receiver;
    ASSERT_TRUE(startTaskAndWait(taskPath, receiver)) << "layer install task did not finish";
    EXPECT_EQ(replyCode(receiver.result), 0LL)
      << receiver.result.value("message").toString().toStdString();

    const auto installed =
      repo->listLocalBy(repo::repoCacheQuery{ .id = "org.example.RuntimeTask" });
    ASSERT_TRUE(installed) << installed.error().message();
    ASSERT_EQ(installed->size(), 1);
    EXPECT_EQ(installed->front().info.packageInfoV2Module, "runtime");
}

TEST_F(PackageManagerCommandTest, SetConfigurationUpdatesRepoConfiguration)
{
    auto updated = config;
    updated.repos.front().url = "https://example.com/updated-repo";
    updated.repos.front().priority = 10;

    QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
    auto reply = iface.call("SetConfiguration", common::serialize::toQVariantMap(updated));
    ASSERT_EQ(reply.type(), QDBusMessage::ReplyMessage) << reply.errorMessage().toStdString();

    const auto current = pm->getConfiguration();
    EXPECT_EQ(current.value("defaultRepo").toString(), QStringLiteral("stable"));
    const auto repos = current.value("repos").toList();
    ASSERT_EQ(repos.size(), 1);
    EXPECT_EQ(repos.front().toMap().value("url").toString(),
              QStringLiteral("https://example.com/updated-repo"));
    EXPECT_EQ(repos.front().toMap().value("priority").toInt(), 10);
}

TEST_F(PackageManagerCommandTest, SetConfigurationIgnoresUnchangedConfiguration)
{
    QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
    auto reply = iface.call("SetConfiguration", common::serialize::toQVariantMap(config));
    EXPECT_EQ(reply.type(), QDBusMessage::ReplyMessage);
    EXPECT_EQ(pm->getConfiguration().value("defaultRepo").toString(), QStringLiteral("stable"));
}

} // namespace

namespace {

QDBusMessage callCommandRaw(const QString &serviceName,
                            const QString &method,
                            const QVariantMap &params,
                            const QDBusConnection &connection)
{
    QDBusInterface iface(serviceName, kObjectPath, kInterfaceName, connection);
    return iface.call(method, params);
}

} // namespace

TEST_F(PackageManagerCommandTest, SetConfigurationRejectsMissingDefaultRepo)
{
    v1::RepoConfigV2 badConfig{
        .defaultRepo = "ghost",
        .repos = { v1::Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };

    auto reply = callCommandRaw(serviceName,
                                "SetConfiguration",
                                common::serialize::toQVariantMap(badConfig),
                                connection);

    EXPECT_EQ(reply.type(), QDBusMessage::ErrorMessage);
    EXPECT_EQ(pm->getConfiguration().value("defaultRepo").toString(), QStringLiteral("stable"));
}

TEST_F(PackageManagerCommandTest, SetConfigurationRejectsMalformedParameters)
{
    QVariantMap malformed;
    malformed.insert("version", QStringLiteral("not-a-number"));

    auto reply = callCommandRaw(serviceName, "SetConfiguration", malformed, connection);

    EXPECT_EQ(reply.type(), QDBusMessage::ErrorMessage);
    EXPECT_EQ(pm->getConfiguration().value("defaultRepo").toString(), QStringLiteral("stable"));
}

TEST_F(PackageManagerCommandTest, UninstallQueuesInstalledAppForRemoval)
{
    test::ScopedRepoLockFile lockFile;
    if (!lockFile.ok()) {
        GTEST_SKIP() << lockFile.error();
    }
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return std::vector<v1::RepositoryCacheLayersItem>{
            makeLayerItem("1.0.0", "app", "binary")
        };
    };

    auto params = v1::PackageManager1UninstallParameters{
        .options = v1::CommonOptions{ .force = true, .noAutoPrune = true, .skipInteraction = true },
        .package = v1::PackageManager1Package{ .channel = "main",
                                               .id = "org.example.App",
                                               .packageManager1PackageModule = "binary",
                                               .version = "1.0.0" },
    };

    auto result = callCommand("Uninstall", common::serialize::toQVariantMap(params));
    EXPECT_EQ(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(), ::testing::HasSubstr("uninstalling"));
}

TEST_F(PackageManagerCommandTest, SearchSkipsUnknownRepository)
{
    auto result = callCommand(
      "Search",
      common::serialize::toQVariantMap(
        v1::PackageManager1SearchParameters{ .id = "org.example.App", .repos = { "ghost" } }));

    EXPECT_EQ(replyCode(result), 0LL);
    EXPECT_THAT(replyMessage(result).toStdString(), ::testing::HasSubstr("waiting to be searched"));
}

TEST_F(PackageManagerCommandTest, PruneCompletesAndReturnsEmptyPackageList)
{
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return std::vector<v1::RepositoryCacheLayersItem>{};
    };

    auto result = callCommandWithoutArguments("Prune");
    ASSERT_EQ(replyCode(result), 0LL);
    const auto taskPath = result.value("taskObjectPath").toString();
    ASSERT_FALSE(taskPath.isEmpty());

    TaskResultReceiver receiver;
    ASSERT_TRUE(connection.connect(serviceName,
                                   taskPath,
                                   "org.deepin.linglong.Task1",
                                   "TaskFinished",
                                   &receiver,
                                   SLOT(onTaskFinished(QVariantMap))));

    QDBusInterface taskIface(serviceName, taskPath, "org.deepin.linglong.Task1", connection);
    ASSERT_TRUE(taskIface.isValid());
    ASSERT_NE(taskIface.call("Start").type(), QDBusMessage::ErrorMessage);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!receiver.finished && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(receiver.finished) << "prune task did not finish";
    EXPECT_EQ(replyCode(receiver.result), 0LL);
    EXPECT_EQ(receiver.result.value("type").toString(),
              QStringLiteral("PackageManager1PruneResult"));
    EXPECT_TRUE(receiver.result.value("packages").toList().isEmpty());
}

TEST_F(PackageManagerCommandTest, PruneReportsLocalQueryFailure)
{
    repo->onListLocalBy = [](const repo::repoCacheQuery &) {
        return utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>(
          tl::make_unexpected(utils::error::Error::Err(__FILE__, __LINE__, "", "query down")));
    };

    auto result = callCommandWithoutArguments("Prune");
    ASSERT_EQ(replyCode(result), 0LL);
    const auto taskPath = result.value("taskObjectPath").toString();
    ASSERT_FALSE(taskPath.isEmpty());

    TaskResultReceiver receiver;
    ASSERT_TRUE(connection.connect(serviceName,
                                   taskPath,
                                   "org.deepin.linglong.Task1",
                                   "TaskFinished",
                                   &receiver,
                                   SLOT(onTaskFinished(QVariantMap))));

    QDBusInterface taskIface(serviceName, taskPath, "org.deepin.linglong.Task1", connection);
    ASSERT_TRUE(taskIface.isValid());
    ASSERT_NE(taskIface.call("Start").type(), QDBusMessage::ErrorMessage);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!receiver.finished && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(receiver.finished) << "prune task did not finish";
    EXPECT_NE(replyCode(receiver.result), 0LL);
    EXPECT_THAT(receiver.result.value("message").toString().toStdString(),
                ::testing::HasSubstr("query down"));
}

TEST_F(PackageManagerCommandTest, SearchContinuesWhenRemoteQueryFails)
{
    repo->onSearchRemote = [](std::string, const v1::Repo &) {
        return utils::error::Result<std::vector<v1::PackageInfoV2>>(
          tl::make_unexpected(utils::error::Error::Err(__FILE__, __LINE__, "", "remote down")));
    };

    auto result = callCommand(
      "Search",
      common::serialize::toQVariantMap(
        v1::PackageManager1SearchParameters{ .id = "org.example.App", .repos = { "stable" } }));

    EXPECT_EQ(replyCode(result), 0LL);
}

#include "package_manager_command_test.moc"
