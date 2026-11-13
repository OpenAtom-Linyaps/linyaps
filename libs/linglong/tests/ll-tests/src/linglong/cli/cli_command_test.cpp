/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/scoped_runtime_paths.h"
#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/cli.h"
#include "linglong/cli/cli_printer.h"
#include "linglong/cli/printer.h"
#include "linglong/common/cli/repo.h"
#include "linglong/common/serialize/json.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/env.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QTimer>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using namespace linglong;
namespace v1 = api::types::v1;

constexpr const char *kObjectPath = "/org/deepin/linglong/PackageManager1";

v1::RepoConfigV2 makeRepoConfig()
{
    return v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { v1::Repo{
          .name = "stable",
          .priority = 0,
          .url = "https://example.com/repo",
        } },
        .version = 2,
    };
}

v1::PackageInfoV2 makePackage(const std::string &id,
                              const std::string &version,
                              const std::string &kind,
                              const std::string &module)
{
    return v1::PackageInfoV2{
        .arch = { "x86_64" },
        .channel = "main",
        .id = id,
        .kind = kind,
        .packageInfoV2Module = module,
        .name = id,
        .version = version,
    };
}

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

class FakeTask : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.Task1")

public:
    explicit FakeTask(QVariantMap result, QObject *parent = nullptr)
        : QObject(parent)
        , finishedResult(std::move(result))
    {
    }

    QVariantMap finishedResult;
    bool interactionEnabled{ false };
    bool interactionReplied{ false };
    QString interactionId{ "interaction-1" };
    int interactionMessageId{ static_cast<int>(v1::InteractionMessageType::Upgrade) };
    QVariantMap interactionMessage;
    QVariantMap lastInteractionReply;

public Q_SLOTS:

    void Start()
    {
        if (interactionEnabled) {
            QTimer::singleShot(0, this, [this]() {
                Q_EMIT RequestInteraction(interactionId, interactionMessageId, interactionMessage);
                QTimer::singleShot(200, this, [this]() {
                    this->finish();
                });
            });
            return;
        }

        QTimer::singleShot(0, this, [this]() {
            this->finish();
        });
    }

    void Cancel() { }

    void ReplyInteraction(const QString &id, const QVariantMap &replies)
    {
        interactionReplied = true;
        lastInteractionReply = replies;
        Q_UNUSED(id);
        QTimer::singleShot(0, this, [this]() {
            this->finish();
        });
    }

Q_SIGNALS:
    void RequestInteraction(const QString &interactionId,
                            int messageID,
                            const QVariantMap &additionalMessage);
    void TaskEvent(const QString &event, const QVariantMap &data);
    void TaskFinished(const QVariantMap &result);

private:
    void finish()
    {
        if (finished) {
            return;
        }
        finished = true;
        Q_EMIT TaskEvent(QStringLiteral("state"),
                         common::serialize::toQVariantMap(v1::TaskState{
                           .message = "working",
                           .progress = 42.0,
                           .state = v1::State::Processing,
                         }));
        Q_EMIT TaskEvent(QStringLiteral("state"),
                         common::serialize::toQVariantMap(v1::TaskState{
                           .message = "done",
                           .progress = 100.0,
                           .state = v1::State::Succeed,
                         }));
        Q_EMIT TaskFinished(finishedResult);
    }

    bool finished{ false };
};

class FakePackageManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManager1")
    Q_PROPERTY(QVariantMap Configuration READ configuration)

public:
    explicit FakePackageManager(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QVariantMap configuration() const { return configValue; }

    QVariantMap configValue{ common::serialize::toQVariantMap(makeRepoConfig()) };
    QString taskPath;
    QString lastMethod;
    QVariantMap lastParams;
    int errorCode{ 0 };
    std::string errorMessage{ "failed" };
    bool failSetConfiguration{ false };
    QVariantMap finishedResult;
    std::map<std::string, std::vector<v1::PackageInfoV2>> searchPackages;
    std::optional<std::vector<v1::PackageInfoV2>> prunePackages;

public Q_SLOTS:

    QVariantMap Install(const QVariantMap &parameters)
    {
        lastMethod = "Install";
        lastParams = parameters;
        return taskCreated();
    }

    QVariantMap InstallFromFile(const QDBusUnixFileDescriptor &fd,
                                const QString &fileType,
                                const QVariantMap &options)
    {
        lastMethod = "InstallFromFile";
        lastParams = options;
        lastParams.insert("fileType", fileType);
        fd.fileDescriptor();
        return taskCreated();
    }

    QVariantMap Uninstall(const QVariantMap &parameters)
    {
        lastMethod = "Uninstall";
        lastParams = parameters;
        return taskCreated();
    }

    QVariantMap Update(const QVariantMap &parameters)
    {
        lastMethod = "Update";
        lastParams = parameters;
        return taskCreated();
    }

    QVariantMap Search(const QVariantMap &parameters)
    {
        lastMethod = "Search";
        lastParams = parameters;
        return taskCreated();
    }

    QVariantMap Prune()
    {
        lastMethod = "Prune";
        return taskCreated();
    }

    QVariantMap InitRunContext(const QString &runContextCfg, const QString &containerID)
    {
        lastMethod = "InitRunContext";
        lastParams.insert("config", runContextCfg);
        lastParams.insert("containerID", containerID);
        return taskCreated();
    }

    void SetConfiguration(const QVariantMap &parameters)
    {
        lastMethod = "SetConfiguration";
        lastParams = parameters;
        if (failSetConfiguration) {
            sendErrorReply(QDBusError::AccessDenied, "not allowed to change configuration");
        }
    }

private:
    QVariantMap taskCreated()
    {
        if (errorCode != 0) {
            return common::serialize::toQVariantMap(v1::PackageManager1PackageTaskResult{
              .taskObjectPath = std::nullopt,
              .code = errorCode,
              .message = errorMessage,
            });
        }

        return common::serialize::toQVariantMap(v1::PackageManager1PackageTaskResult{
          .taskObjectPath = taskPath.toStdString(),
          .code = 0,
          .message = "task created",
        });
    }
};

class MockRepo : public repo::OSTreeRepo
{
public:
    explicit MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(path, makeRepoConfig())
    {
    }

    bool initialize() { return this->init(true).has_value(); }

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReferenceLocal,
                (const package::FuzzyReference &, bool),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::LayerDir>,
                getLayerDir,
                (const package::Reference &, const std::string &),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &, std::string),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<repo::RemotePackages>,
                matchRemoteByPriority,
                (const package::FuzzyReference &, const std::optional<v1::Repo> &),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<v1::PackageInfoV2>,
                fetchRemotePackageInfo,
                (const package::ReferenceWithRepo &, const std::string &),
                (override, noexcept));
    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &),
                (override, const, noexcept));
};

class MockPrinter : public cli::CLIPrinter
{
public:
    MOCK_METHOD(void, printErr, (const utils::error::Error &), (override));
    MOCK_METHOD(void, printMessage, (const std::string &), (override));
    MOCK_METHOD(void, printProgress, (double, const std::string &), (override));
    MOCK_METHOD(void, finishProgress, (), (override));
    MOCK_METHOD(void,
                printSearchResult,
                ((std::map<std::string, std::vector<v1::PackageInfoV2>>)),
                (override));
    MOCK_METHOD(void, printPruneResult, (const std::vector<v1::PackageInfoV2> &), (override));
    MOCK_METHOD(void, printRepoConfig, (const v1::RepoConfigV2 &), (override));
    MOCK_METHOD(void, printPackage, (const v1::PackageInfoV2 &), (override));
    MOCK_METHOD(void, printPackages, (const std::vector<v1::PackageInfoDisplay> &), (override));
    MOCK_METHOD(void, printLayerInfo, (const v1::LayerInfo &), (override));
    MOCK_METHOD(void, printContent, (const QStringList &), (override));
    MOCK_METHOD(void, printUpgradeList, (std::vector<v1::UpgradeListResult> &), (override));
    MOCK_METHOD(void,
                printModuleSizes,
                (const std::vector<cli::Printer::ModuleSizeInfo> &, std::uint64_t, std::uint64_t),
                (override));
    MOCK_METHOD(void, printDepends, (const std::vector<cli::Printer::DependsNode> &), (override));
    MOCK_METHOD(void, printContainers, (const std::vector<v1::CliContainer> &), (override));
};

class MockNotifier : public cli::InteractiveNotifier
{
public:
    MOCK_METHOD(utils::error::Result<v1::InteractionReply>,
                request,
                (const v1::InteractionRequest &),
                (override));
    MOCK_METHOD(utils::error::Result<void>, notify, (const v1::InteractionRequest &), (override));
};

class TestCli : public cli::Cli
{
public:
    using cli::Cli::Cli;

    void setRepo(repo::OSTreeRepo *value) { repoPtr = value; }

    void setPkgMan(api::dbus::v1::PackageManager *value) { pkgManPtr = value; }

protected:
    utils::error::Result<repo::OSTreeRepo *> getRepo(bool) noexcept override
    {
        if (repoPtr == nullptr) {
            LINGLONG_TRACE("get repo");
            return LINGLONG_ERR("no repo configured");
        }
        return repoPtr;
    }

    utils::error::Result<api::dbus::v1::PackageManager *> getPkgMan() override
    {
        if (pkgManPtr == nullptr) {
            LINGLONG_TRACE("get package manager");
            return LINGLONG_ERR("no package manager");
        }
        return pkgManPtr;
    }

private:
    repo::OSTreeRepo *repoPtr{ nullptr };
    api::dbus::v1::PackageManager *pkgManPtr{ nullptr };
};

class CliCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto &shared = sharedBus();
        if (!shared.ready) {
            GTEST_SKIP() << "a private dbus-daemon is required for cli command tests";
        }

        if (QCoreApplication::instance() == nullptr) {
            static int argc = 0;
            application = std::make_unique<QCoreApplication>(argc, nullptr);
        }

        connectionName = QString("linglong-cli-test-%1").arg(instanceCounter++);
        connection =
          QDBusConnection::connectToBus(QString::fromStdString(shared.daemon.busAddress()),
                                        connectionName);
        if (!connection.isConnected()) {
            GTEST_SKIP() << "failed to connect to the private session bus";
        }
        serviceName = QString("org.deepin.linglong.CliCommandTest.T%1").arg(instanceCounter++);

        if (!connection.registerService(serviceName)) {
            ADD_FAILURE() << "registerService failed for " << serviceName.toStdString() << ": "
                          << connection.lastError().message().toStdString();
        }

        pkgManService = std::make_unique<FakePackageManager>();
        taskPath = QString("/org/deepin/linglong/Task1/%1").arg(instanceCounter++);
        pkgManService->taskPath = taskPath;
        ASSERT_TRUE(connection.registerObject(kObjectPath,
                                              pkgManService.get(),
                                              QDBusConnection::ExportAllSlots
                                                | QDBusConnection::ExportAllSignals
                                                | QDBusConnection::ExportAllProperties));

        tempDir = std::make_unique<TempDir>();
        repoOwner = std::make_unique<MockRepo>(tempDir->path() / "repo");
        ASSERT_TRUE(repoOwner->initialize());

        printer = std::make_unique<::testing::NiceMock<MockPrinter>>();
        auto mockNotifier = std::make_unique<::testing::NiceMock<MockNotifier>>();
        notifier = mockNotifier.get();
        ON_CALL(*notifier, request(testing::_))
          .WillByDefault(testing::Invoke(
            [](const v1::InteractionRequest &) -> utils::error::Result<v1::InteractionReply> {
                return v1::InteractionReply{ .action = "yes" };
            }));
        ON_CALL(*notifier, notify(testing::_))
          .WillByDefault(testing::Invoke([](const v1::InteractionRequest &) {
              return utils::error::Result<void>{};
          }));

        ociCLI = ocppi::cli::crun::Crun::New("/bin/false").value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        pkgManProxy = std::make_unique<api::dbus::v1::PackageManager>(serviceName,
                                                                      kObjectPath,
                                                                      connection,
                                                                      nullptr);
        cli = std::make_unique<TestCli>(*printer,
                                        *ociCLI,
                                        *containerBuilder,
                                        false,
                                        std::move(mockNotifier),
                                        nullptr);
        cli->setRepo(repoOwner.get());
        cli->setPkgMan(pkgManProxy.get());
    }

    void TearDown() override
    {
        clearTask();
        cli.reset();
        pkgManProxy.reset();
        containerBuilder.reset();
        ociCLI.reset();
        printer.reset();
        repoOwner.reset();
        tempDir.reset();
        if (connection.isConnected()) {
            connection.unregisterObject(kObjectPath);
            connection.unregisterService(serviceName);
        }
        pkgManService.reset();
        QDBusConnection::disconnectFromBus(connectionName);
        connection = QDBusConnection(QString());
        // Qt only supports one QCoreApplication per process; keep it alive.
        application.release();
    }

    void publishTask(QVariantMap result, bool interaction = false)
    {
        clearTask();
        taskOwner = std::make_unique<FakeTask>(std::move(result));
        taskOwner->interactionEnabled = interaction;
        ASSERT_TRUE(connection.registerObject(taskPath,
                                              taskOwner.get(),
                                              QDBusConnection::ExportAllSlots
                                                | QDBusConnection::ExportAllSignals));
    }

    void clearTask()
    {
        if (taskOwner) {
            connection.unregisterObject(taskPath);
            taskOwner.reset();
        }
    }

    static QVariantMap commonResult(std::string message = "done")
    {
        return common::serialize::toQVariantMap(v1::CommonResult{
          .code = 0,
          .message = std::move(message),
        });
    }

    void useError(int code, std::string message = "failed")
    {
        pkgManService->errorCode = code;
        pkgManService->errorMessage = std::move(message);
    }

    static std::atomic<int> instanceCounter;
    std::unique_ptr<QCoreApplication> application;
    QString connectionName;
    QString serviceName;
    QString taskPath;
    QDBusConnection connection{ QString() };
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<MockRepo> repoOwner;
    std::unique_ptr<FakePackageManager> pkgManService;
    std::unique_ptr<FakeTask> taskOwner;
    std::unique_ptr<MockPrinter> printer;
    MockNotifier *notifier{ nullptr };
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<api::dbus::v1::PackageManager> pkgManProxy;
    std::unique_ptr<TestCli> cli;
};

std::atomic<int> CliCommandTest::instanceCounter{ 0 };

TEST_F(CliCommandTest, InstallRunsTaskAndPrintsSuccess)
{
    publishTask(commonResult("installed"));

    EXPECT_CALL(*printer, printProgress(testing::DoubleEq(42.0), "working"));
    EXPECT_CALL(*printer, printMessage("installed"));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App", .module = "binary" }),
              0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Install"));
    const auto packageArgument = pkgManService->lastParams.value("package");
    ASSERT_TRUE(packageArgument.canConvert<QDBusArgument>());
    const auto package = qdbus_cast<QVariantMap>(packageArgument.value<QDBusArgument>());
    EXPECT_EQ(package.value("id").toString(), QStringLiteral("org.example.App"));
}

TEST_F(CliCommandTest, InstallFromFileRunsTaskAndPrintsSuccess)
{
    const auto packagePath = tempDir->path() / "org.example.App_1.0_x86_64_binary.layer";
    std::ofstream(packagePath) << "layer placeholder";
    publishTask(commonResult("installed from file"));

    EXPECT_CALL(*printer, printMessage("installed from file"));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = packagePath.string() }), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("InstallFromFile"));
    EXPECT_EQ(pkgManService->lastParams.value("fileType").toString(), QStringLiteral("layer"));
}

TEST_F(CliCommandTest, InstallMapsKnownErrorCodesToUserMessages)
{
    struct Case
    {
        utils::error::ErrorCode code;
        const char *message;
    };

    const std::vector<Case> cases{
        { utils::error::ErrorCode::AppInstallModuleRequireAppFirst,
          "To install the module, one must first install the app." },
        { utils::error::ErrorCode::AppInstallModuleAlreadyExists, "Module is already installed." },
        { utils::error::ErrorCode::AppInstallModuleNotFound,
          "The module could not be found remotely." },
        { utils::error::ErrorCode::AppInstallAlreadyInstalled, "Application already installed" },
        { utils::error::ErrorCode::AppInstallModuleNoVersion,
          "Cannot specify a version when installing a module." },
        { utils::error::ErrorCode::AppInstallFailed, "Install failed" },
        { utils::error::ErrorCode::Unknown, "Install failed" },
    };

    for (const auto &testCase : cases) {
        useError(static_cast<int>(testCase.code));
        EXPECT_CALL(*printer, printMessage(testCase.message)).Times(1);
        EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
        testing::Mock::VerifyAndClearExpectations(printer.get());
    }
}

TEST_F(CliCommandTest, InstallReportsRemoteNotFoundWithPackageId)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppInstallNotFoundFromRemote));

    EXPECT_CALL(*printer, printMessage("Application org.example.App is not found in remote repo."));
    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, InstallReportsNeedDowngradeWithForceHint)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppInstallNeedDowngrade));

    EXPECT_CALL(*printer,
                printMessage(testing::HasSubstr("ll-cli install org.example.App --force")));
    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, InstallMapsCommonErrorCodes)
{
    struct Case
    {
        utils::error::ErrorCode code;
        const char *message;
    };

    const std::vector<Case> cases{
        { utils::error::ErrorCode::NetworkError,
          "Network connection failed. Please:\n1. Check your internet connection\n2. Verify "
          "network proxy settings if used" },
        { utils::error::ErrorCode::LayerCompatibilityError, "Package not found" },
        { utils::error::ErrorCode::Canceled, "Operation canceled" },
        { utils::error::ErrorCode::PermissionDenied,
          "Permission denied, authentication is required" },
    };

    for (const auto &testCase : cases) {
        useError(static_cast<int>(testCase.code));
        EXPECT_CALL(*printer, printMessage(testCase.message)).Times(1);
        EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
        testing::Mock::VerifyAndClearExpectations(printer.get());
    }
}

TEST_F(CliCommandTest, InstallVerbosePrintsUnderlyingError)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppInstallFailed), "disk is full");
    cli->setGlobalOptions(cli::GlobalOptions{ .verbose = 1 });

    EXPECT_CALL(*printer, printMessage("Install failed"));
    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_EQ(error.code(), static_cast<int>(utils::error::ErrorCode::AppInstallFailed));
        EXPECT_THAT(error.message(), testing::HasSubstr("disk is full"));
    });

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, InstallReportsPackageManagerUnavailable)
{
    cli->setPkgMan(nullptr);

    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("no package manager"));
    });
    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, UninstallRunsTaskAndMapsErrorCodes)
{
    publishTask(commonResult("uninstalled"));
    EXPECT_CALL(*printer, printMessage("uninstalled"));
    EXPECT_EQ(cli->uninstall(cli::UninstallOptions{ .appid = "org.example.App" }), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Uninstall"));

    struct Case
    {
        utils::error::ErrorCode code;
        const char *message;
    };

    const std::vector<Case> cases{
        { utils::error::ErrorCode::AppUninstallNotFoundFromLocal, "Application is not installed." },
        { utils::error::ErrorCode::AppUninstallBaseOrRuntime,
          "Base or runtime cannot be uninstalled, please use 'll-cli prune'." },
        { utils::error::ErrorCode::AppUninstallFailed, "Uninstall failed" },
        { utils::error::ErrorCode::Unknown, "Uninstall failed" },
    };

    for (const auto &testCase : cases) {
        useError(static_cast<int>(testCase.code));
        EXPECT_CALL(*printer, printMessage(testCase.message)).Times(1);
        EXPECT_EQ(cli->uninstall(cli::UninstallOptions{ .appid = "org.example.App" }), -1);
        testing::Mock::VerifyAndClearExpectations(printer.get());
    }
}

TEST_F(CliCommandTest, UninstallReportsMultipleVersionsWithDetail)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppUninstallMultipleVersions),
             "org.example.App/1.0.0\norg.example.App/2.0.0");

    EXPECT_CALL(*printer,
                printMessage(testing::AllOf(testing::HasSubstr("Multiple versions"),
                                            testing::HasSubstr("org.example.App/2.0.0"))));
    EXPECT_EQ(cli->uninstall(cli::UninstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, UninstallReportsRunningApplicationThroughNotifier)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppUninstallAppIsRunning));
    EXPECT_CALL(*notifier, notify(testing::_)).Times(1);
    EXPECT_CALL(*printer, printErr(testing::_)).Times(0);

    EXPECT_EQ(cli->uninstall(cli::UninstallOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, UpgradeMapsErrorCodes)
{
    useError(static_cast<int>(utils::error::ErrorCode::AppUpgradeLocalNotFound));
    EXPECT_CALL(*printer, printMessage("Application is not installed."));
    EXPECT_EQ(cli->upgrade(cli::UpgradeOptions{}), -1);
    testing::Mock::VerifyAndClearExpectations(printer.get());

    useError(static_cast<int>(utils::error::ErrorCode::AppUpgradeFailed));
    EXPECT_CALL(*printer, printMessage("Upgrade failed"));
    EXPECT_EQ(cli->upgrade(cli::UpgradeOptions{}), -1);
    testing::Mock::VerifyAndClearExpectations(printer.get());

    useError(static_cast<int>(utils::error::ErrorCode::Unknown));
    EXPECT_CALL(*printer, printMessage("Upgrade failed"));
    EXPECT_EQ(cli->upgrade(cli::UpgradeOptions{}), -1);
}

TEST_F(CliCommandTest, UpgradeRunsTaskAndPrintsSuccess)
{
    const auto ref = package::Reference::parse("stable:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref);
    auto local = *ref;
    EXPECT_CALL(*repoOwner, clearReferenceLocal(testing::_, false))
      .WillOnce(testing::Return(local));
    EXPECT_CALL(*repoOwner, getLayerItem(testing::_, testing::_))
      .WillOnce(testing::Return(v1::RepositoryCacheLayersItem{
        .commit = "commit",
        .deleted = std::nullopt,
        .info = makePackage("org.example.App", "1.0.0", "app", "binary"),
        .repo = "stable",
      }));

    publishTask(commonResult("upgraded"));
    EXPECT_CALL(*printer, printMessage("upgraded"));
    EXPECT_EQ(cli->upgrade(cli::UpgradeOptions{ .appid = "org.example.App" }), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Update"));
}

TEST_F(CliCommandTest, SearchFiltersDevelAndOldVersions)
{
    auto packages = std::map<std::string, std::vector<v1::PackageInfoV2>>{
        { "stable",
          {
            makePackage("org.example.App", "1.0.0", "app", "binary"),
            makePackage("org.example.App", "2.0.0", "app", "binary"),
            makePackage("org.example.App", "2.0.0", "app", "develop"),
            makePackage("org.example.Base", "1.0.0", "base", "binary"),
          } },
    };
    publishTask(common::serialize::toQVariantMap(v1::PackageManager1SearchResult{
      .packages = packages,
      .code = 0,
      .message = "searched",
    }));

    EXPECT_CALL(*printer, printSearchResult(testing::_))
      .WillOnce([](std::map<std::string, std::vector<v1::PackageInfoV2>> result) {
          ASSERT_EQ(result.size(), 1U);
          const auto &found = result.at("stable");
          ASSERT_EQ(found.size(), 1U);
          EXPECT_EQ(found.front().version, "2.0.0");
          EXPECT_EQ(found.front().packageInfoV2Module, "binary");
      });

    EXPECT_EQ(cli->search(cli::SearchOptions{ .appid = "App", .type = "app" }), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Search"));
}

TEST_F(CliCommandTest, SearchKeepsDevelModulesAndAllVersionsWhenRequested)
{
    auto packages = std::map<std::string, std::vector<v1::PackageInfoV2>>{
        { "stable",
          {
            makePackage("org.example.App", "1.0.0", "app", "binary"),
            makePackage("org.example.App", "2.0.0", "app", "develop"),
          } },
    };
    publishTask(common::serialize::toQVariantMap(v1::PackageManager1SearchResult{
      .packages = packages,
      .code = 0,
      .message = "searched",
    }));

    EXPECT_CALL(*printer, printSearchResult(testing::_))
      .WillOnce([](std::map<std::string, std::vector<v1::PackageInfoV2>> result) {
          ASSERT_EQ(result.size(), 1U);
          EXPECT_EQ(result.at("stable").size(), 2U);
      });

    EXPECT_EQ(cli->search(cli::SearchOptions{
                .appid = "App",
                .type = "all",
                .showDevel = true,
                .showAllVersion = true,
              }),
              0);
}

TEST_F(CliCommandTest, SearchWithoutPackagesPrintsEmptyResult)
{
    publishTask(common::serialize::toQVariantMap(v1::PackageManager1SearchResult{
      .packages = std::nullopt,
      .code = 0,
      .message = "nothing",
    }));

    EXPECT_CALL(*printer, printSearchResult(testing::IsEmpty()));
    EXPECT_EQ(cli->search(cli::SearchOptions{ .appid = "missing" }), 0);
}

TEST_F(CliCommandTest, PrunePrintsRemovedPackages)
{
    publishTask(common::serialize::toQVariantMap(v1::PackageManager1PruneResult{
      .packages =
        std::vector<v1::PackageInfoV2>{
          makePackage("org.example.App", "1.0.0", "app", "binary"),
        },
      .code = 0,
      .message = "pruned",
    }));

    EXPECT_CALL(*printer, printPruneResult(testing::SizeIs(1U)));
    EXPECT_EQ(cli->prune(), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Prune"));
}

TEST_F(CliCommandTest, PruneReportsNothingToDo)
{
    publishTask(common::serialize::toQVariantMap(v1::PackageManager1PruneResult{
      .packages = std::nullopt,
      .code = 0,
      .message = "nothing",
    }));

    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("No packages to prune."));
    });
    EXPECT_EQ(cli->prune(), 0);
}

TEST_F(CliCommandTest, PruneReportsMalformedTaskResult)
{
    publishTask(QVariantMap{ { QStringLiteral("code"), QStringLiteral("not-a-number") } });

    EXPECT_CALL(*printer, printErr(testing::_));
    EXPECT_EQ(cli->prune(), -1);
}

TEST_F(CliCommandTest, RepoShowPrintsConfiguration)
{
    CLI::App app;
    common::cli::RepoOptions options;
    auto *repoCommand = common::cli::addRepoCommand(app, options, "", CLI::Validator{}, "ll-cli");
    app.parse("repo show");

    EXPECT_CALL(*printer, printRepoConfig(testing::_)).WillOnce([](const v1::RepoConfigV2 &config) {
        EXPECT_EQ(config.defaultRepo, "stable");
        ASSERT_EQ(config.repos.size(), 1U);
    });
    EXPECT_EQ(cli->repo(repoCommand, options), 0);
}

TEST_F(CliCommandTest, RepoAddPushesUpdatedConfigurationToPackageManager)
{
    CLI::App app;
    common::cli::RepoOptions options;
    auto *repoCommand = common::cli::addRepoCommand(app, options, "", CLI::Validator{}, "ll-cli");
    app.parse("repo add testing https://example.org");

    EXPECT_EQ(cli->repo(repoCommand, options), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("SetConfiguration"));

    const auto cfg =
      common::serialize::fromQVariantMap<v1::RepoConfigV2>(pkgManService->lastParams);
    ASSERT_TRUE(cfg);
    ASSERT_EQ(cfg->repos.size(), 2U);
    EXPECT_EQ(cfg->repos.back().name, "testing");
}

TEST_F(CliCommandTest, RepoAddReportsPackageManagerConfigurationFailure)
{
    pkgManService->failSetConfiguration = true;

    CLI::App app;
    common::cli::RepoOptions options;
    auto *repoCommand = common::cli::addRepoCommand(app, options, "", CLI::Validator{}, "ll-cli");
    app.parse("repo add testing https://example.org");

    EXPECT_CALL(*printer, printMessage("Permission denied, authentication is required"));
    EXPECT_EQ(cli->repo(repoCommand, options), -1);
}

TEST_F(CliCommandTest, InspectDirPrintsLayerPath)
{
    CLI::App app;
    app.add_subcommand("dir");
    app.add_subcommand("remote-info");
    app.parse("dir");

    const auto ref = package::Reference::parse("stable:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref);
    EXPECT_CALL(*repoOwner, clearReferenceLocal(testing::_, false)).WillOnce(testing::Return(*ref));
    const std::filesystem::path layerPath = tempDir->path() / "layer";
    EXPECT_CALL(*repoOwner, getLayerDir(testing::_, "binary"))
      .WillOnce(testing::Return(package::LayerDir{ layerPath }));

    EXPECT_EQ(cli->inspect(&app, cli::InspectOptions{ .appid = "org.example.App" }), 0);
}

TEST_F(CliCommandTest, InspectDirReportsMissingReference)
{
    CLI::App app;
    app.add_subcommand("dir");
    app.add_subcommand("remote-info");
    app.parse("dir");

    EXPECT_CALL(*repoOwner, clearReferenceLocal(testing::_, false))
      .WillOnce(testing::Invoke([]() -> utils::error::Result<package::Reference> {
          LINGLONG_TRACE("missing reference");
          return LINGLONG_ERR("not found");
      }));
    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("Can not find such application."));
    });

    EXPECT_EQ(cli->inspect(&app, cli::InspectOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, InspectDirRejectsUnknownDirType)
{
    CLI::App app;
    app.add_subcommand("dir");
    app.add_subcommand("remote-info");
    app.parse("dir");

    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("type must be layer or bundle"));
    });
    EXPECT_EQ(
      cli->inspect(&app, cli::InspectOptions{ .appid = "org.example.App", .dirType = "other" }),
      -1);
}

TEST_F(CliCommandTest, RunRejectsInvalidFuzzyReference)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_EQ(error.code(), static_cast<int>(utils::error::ErrorCode::InvalidFuzzyReference));
    });
    EXPECT_EQ(cli->run(cli::RunOptions{ .appid = "/invalid//" }), -1);
}

TEST_F(CliCommandTest, RunReportsRepoError)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    cli->setRepo(nullptr);
    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("no repo configured"));
    });
    EXPECT_EQ(cli->run(cli::RunOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, RunReportsLocalReferenceError)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    EXPECT_CALL(*repoOwner, clearReferenceLocal(testing::_, false))
      .WillOnce(testing::Invoke([]() -> utils::error::Result<package::Reference> {
          LINGLONG_TRACE("local reference");
          return LINGLONG_ERR("not installed");
      }));
    EXPECT_CALL(*printer, printErr(testing::_)).WillOnce([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), testing::HasSubstr("not installed"));
    });
    EXPECT_EQ(cli->run(cli::RunOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, InteractionRepliesToTaskWithNotifierDecision)
{
    publishTask(commonResult("installed"), true);
    taskOwner->interactionMessage =
      common::serialize::toQVariantMap(v1::PackageManager1RequestInteractionAdditionalMessage{
        .localRef = "stable:org.example.App/1.0.0/x86_64",
        .remoteRef = "stable:org.example.App/2.0.0/x86_64",
      });

    EXPECT_CALL(*notifier, request(testing::_)).WillOnce([](const v1::InteractionRequest &request) {
        EXPECT_THAT(request.body.value_or(""), testing::HasSubstr("1.0.0"));
        EXPECT_THAT(request.body.value_or(""), testing::HasSubstr("2.0.0"));
        return v1::InteractionReply{ .action = "yes" };
    });
    EXPECT_CALL(*printer, printMessage("installed"));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), 0);
    EXPECT_TRUE(taskOwner->interactionReplied);
    EXPECT_EQ(taskOwner->lastInteractionReply.value("action").toString(), QStringLiteral("yes"));
}

TEST_F(CliCommandTest, InteractionTreatsUnknownMessageAsUnknownRequest)
{
    publishTask(commonResult("installed"), true);
    taskOwner->interactionMessageId = static_cast<int>(v1::InteractionMessageType::Install);
    taskOwner->interactionMessage =
      common::serialize::toQVariantMap(v1::PackageManager1RequestInteractionAdditionalMessage{
        .localRef = "stable:org.example.App/1.0.0/x86_64",
        .remoteRef = "stable:org.example.App/2.0.0/x86_64",
      });

    EXPECT_CALL(*notifier, request(testing::_)).WillOnce([](const v1::InteractionRequest &request) {
        EXPECT_EQ(request.body.value_or(""), "unknown interaction type");
        return v1::InteractionReply{ .action = "no" };
    });

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), 0);
    EXPECT_EQ(taskOwner->lastInteractionReply.value("action").toString(), QStringLiteral("no"));
}

TEST_F(CliCommandTest, InteractionFallsBackToNoWhenNotifierFails)
{
    publishTask(commonResult("installed"), true);
    taskOwner->interactionMessage =
      common::serialize::toQVariantMap(v1::PackageManager1RequestInteractionAdditionalMessage{
        .localRef = "stable:org.example.App/1.0.0/x86_64",
        .remoteRef = "stable:org.example.App/2.0.0/x86_64",
      });
    EXPECT_CALL(*notifier, request(testing::_))
      .WillOnce(testing::Invoke([]() -> utils::error::Result<v1::InteractionReply> {
          LINGLONG_TRACE("notifier");
          return LINGLONG_ERR("no display");
      }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), 0);
    EXPECT_EQ(taskOwner->lastInteractionReply.value("action").toString(), QStringLiteral("no"));
}

TEST_F(CliCommandTest, OnTaskEventIgnoresUnknownAndMalformedEvents)
{
    EXPECT_CALL(*printer, printProgress(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*printer, printMessage(testing::_)).Times(0);

    EXPECT_TRUE(QMetaObject::invokeMethod(cli.get(),
                                          "onTaskEvent",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("unknown")),
                                          Q_ARG(QVariantMap, QVariantMap{})));
    EXPECT_TRUE(QMetaObject::invokeMethod(cli.get(),
                                          "onTaskEvent",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("state")),
                                          Q_ARG(QVariantMap, QVariantMap{})));
    const QVariantMap malformedMessage{
        { QStringLiteral("message"), QVariantMap{} },
    };
    EXPECT_TRUE(QMetaObject::invokeMethod(cli.get(),
                                          "onTaskEvent",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("message")),
                                          Q_ARG(QVariantMap, malformedMessage)));
}

TEST_F(CliCommandTest, ListEmptyRepositoryPrintsEmptyPackageList)
{
    EXPECT_CALL(*printer, printPackages(::testing::IsEmpty()));

    EXPECT_EQ(cli->list({}), 0);
}

TEST_F(CliCommandTest, ListUpgradablePrintsEmptyUpgradeList)
{
    linglong::cli::ListOptions options;
    options.showUpgradeList = true;
    EXPECT_CALL(*printer, printUpgradeList(::testing::_));

    EXPECT_EQ(cli->list(options), 0);
}

TEST_F(CliCommandTest, SizeEmptyRepositoryPrintsZeroSizes)
{
    EXPECT_CALL(*printer, printModuleSizes(::testing::IsEmpty(), 0, ::testing::_));

    EXPECT_EQ(cli->size({}), 0);
}

TEST_F(CliCommandTest, DependsWithoutAppidPrintsEmptyTree)
{
    EXPECT_CALL(*printer, printDepends(::testing::IsEmpty()));

    EXPECT_EQ(cli->depends({}), 0);
}

TEST_F(CliCommandTest, InfoEmptyAppIdReportsError)
{
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_NE(cli->info({}), 0);
}

TEST_F(CliCommandTest, InfoRejectsInvalidFuzzyReference)
{
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->info({ "org.example/1.0.0/x86_64/extra" }), -1);
}

TEST_F(CliCommandTest, InfoReportsMissingLocalApplication)
{
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          LINGLONG_TRACE("clearReferenceLocal");
          return LINGLONG_ERR("not installed");
      }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->info({ "org.example.Missing" }), -1);
}

TEST_F(CliCommandTest, InfoRejectsBrokenLayerFile)
{
    auto path = tempDir->path() / "broken.layer";
    std::ofstream{ path } << "not an erofs layer";
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->info({ path.string() }), -1);
}

TEST_F(CliCommandTest, InfoPrintsPackageInfoFromLayerDir)
{
    auto ref = package::Reference::parse("main:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref) << ref.error().message();
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          return utils::error::Result<package::Reference>{
              package::Reference::parse("main:org.example.App/1.0.0/x86_64").value()
          };
      }));

    auto layerDir = tempDir->path() / "layer";
    std::filesystem::create_directories(layerDir);
    v1::PackageInfoV2 info{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "org.example.App",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };
    std::ofstream{ layerDir / "info.json" } << nlohmann::json(info).dump();
    ON_CALL(*repoOwner, getLayerDir(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([layerDir](const package::Reference &, const std::string &) {
          return utils::error::Result<package::LayerDir>{ package::LayerDir{ layerDir } };
      }));
    EXPECT_CALL(*printer, printPackage(::testing::_));

    EXPECT_EQ(cli->info({ "org.example.App" }), 0);
}

TEST_F(CliCommandTest, InfoReportsUnreadableLayerMetadata)
{
    auto ref = package::Reference::parse("main:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref) << ref.error().message();
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          return utils::error::Result<package::Reference>{
              package::Reference::parse("main:org.example.App/1.0.0/x86_64").value()
          };
      }));
    auto layerDir = tempDir->path() / "empty-layer";
    std::filesystem::create_directories(layerDir);
    ON_CALL(*repoOwner, getLayerDir(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([layerDir](const package::Reference &, const std::string &) {
          return utils::error::Result<package::LayerDir>{ package::LayerDir{ layerDir } };
      }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->info({ "org.example.App" }), -1);
}

TEST_F(CliCommandTest, ContentRejectsInvalidFuzzyReference)
{
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->content({ "org.example/1.0.0/x86_64/extra" }), -1);
}

TEST_F(CliCommandTest, ContentReportsMissingLocalApplication)
{
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          LINGLONG_TRACE("clearReferenceLocal");
          return LINGLONG_ERR("not installed");
      }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->content({ "org.example.Missing" }), -1);
}

TEST_F(CliCommandTest, ContentRejectsNonAppLayer)
{
    auto ref = package::Reference::parse("main:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref) << ref.error().message();
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          return utils::error::Result<package::Reference>{
              package::Reference::parse("main:org.example.App/1.0.0/x86_64").value()
          };
      }));
    ON_CALL(*repoOwner, getLayerItem(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::Reference &reference, std::string) {
          return utils::error::Result<v1::RepositoryCacheLayersItem>{ v1::RepositoryCacheLayersItem{
            .commit = "commit",
            .deleted = std::nullopt,
            .info =
              v1::PackageInfoV2{
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "org.example.App",
                .kind = "runtime",
                .packageInfoV2Module = "binary",
                .version = "1.0.0",
              },
            .repo = "stable",
          } };
      }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->content({ "org.example.App" }), -1);
}

TEST_F(CliCommandTest, ContentReportsMissingEntriesDirectory)
{
    auto ref = package::Reference::parse("main:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref) << ref.error().message();
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          return utils::error::Result<package::Reference>{
              package::Reference::parse("main:org.example.App/1.0.0/x86_64").value()
          };
      }));
    ON_CALL(*repoOwner, getLayerItem(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::Reference &, std::string) {
          return utils::error::Result<v1::RepositoryCacheLayersItem>{ v1::RepositoryCacheLayersItem{
            .commit = "commit",
            .deleted = std::nullopt,
            .info =
              v1::PackageInfoV2{
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "org.example.App",
                .kind = "app",
                .packageInfoV2Module = "binary",
                .version = "1.0.0",
              },
            .repo = "stable",
          } };
      }));
    auto layerDir = tempDir->path() / "app-layer";
    std::filesystem::create_directories(layerDir);
    ON_CALL(*repoOwner, getLayerDir(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([layerDir](const package::Reference &, const std::string &) {
          return utils::error::Result<package::LayerDir>{ package::LayerDir{ layerDir } };
      }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->content({ "org.example.App" }), -1);
}

TEST_F(CliCommandTest, ContentPrintsExportedEntries)
{
    auto ref = package::Reference::parse("main:org.example.App/1.0.0/x86_64");
    ASSERT_TRUE(ref) << ref.error().message();
    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          return utils::error::Result<package::Reference>{
              package::Reference::parse("main:org.example.App/1.0.0/x86_64").value()
          };
      }));
    ON_CALL(*repoOwner, getLayerItem(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::Reference &, std::string) {
          return utils::error::Result<v1::RepositoryCacheLayersItem>{ v1::RepositoryCacheLayersItem{
            .commit = "commit",
            .deleted = std::nullopt,
            .info =
              v1::PackageInfoV2{
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "org.example.App",
                .kind = "app",
                .packageInfoV2Module = "binary",
                .version = "1.0.0",
              },
            .repo = "stable",
          } };
      }));
    auto layerDir = tempDir->path() / "app-layer";
    std::filesystem::create_directories(layerDir / "entries" / "usr/share/applications");
    std::filesystem::create_directories(layerDir / "entries" / "lib/systemd/user");
    std::ofstream{ layerDir / "entries" / "usr/share/applications/org.example.App.desktop" }
      << "[Desktop Entry]\nName=App\n";
    ON_CALL(*repoOwner, getLayerDir(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([layerDir](const package::Reference &, const std::string &) {
          return utils::error::Result<package::LayerDir>{ package::LayerDir{ layerDir } };
      }));
    EXPECT_CALL(*printer, printContent(::testing::_));

    EXPECT_EQ(cli->content({ "org.example.App" }), 0);
}

TEST_F(CliCommandTest, RemoteInfoRejectsInvalidFuzzyReference)
{
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->remoteInfo({ .appid = "org.example/1.0.0/x86_64/extra" }), -1);
}

TEST_F(CliCommandTest, RemoteInfoRejectsUnknownRepoAlias)
{
    linglong::cli::InspectOptions options;
    options.appid = "org.example.App";
    options.repo = "missing";
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->remoteInfo(options), -1);
}

TEST_F(CliCommandTest, RemoteInfoReportsRemoteLookupFailure)
{
    ON_CALL(*repoOwner, matchRemoteByPriority(::testing::_, ::testing::_))
      .WillByDefault(
        ::testing::Invoke([](const package::FuzzyReference &, const std::optional<v1::Repo> &) {
            LINGLONG_TRACE("matchRemoteByPriority");
            return LINGLONG_ERR("remote lookup failed");
        }));
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->remoteInfo({ .appid = "org.example.App" }), -1);
}

TEST_F(CliCommandTest, SizeReportsImportedModulesAndSortOrders)
{
    const auto repoRoot = tempDir->path() / "size-repo";
    std::filesystem::create_directories(repoRoot);
    auto created = repo::OSTreeRepo::create(repoRoot, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto realRepo = std::move(*created);

    auto importLayer = [&](const std::string &module,
                           const std::vector<std::string> &files) -> bool {
        const auto layerDir = tempDir->path() / ("size-layer-" + module);
        std::filesystem::create_directories(layerDir);
        for (const auto &file : files) {
            const auto path = layerDir / file;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path) << std::string(4096, 'x');
        }

        auto info = makePackage("org.example.size", "1.0.0", "app", module);
        std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();

        package::LayerDir dir{ layerDir };
        if (!dir.valid()) {
            return false;
        }
        return realRepo->importLayerDir(dir).has_value();
    };

    ASSERT_TRUE(importLayer("binary", { "files/usr/bin/size-app", "files/share/size.txt" }));
    ASSERT_TRUE(importLayer("develop", { "files/usr/include/size.h" }));

    cli->setRepo(realRepo.get());

    std::vector<cli::Printer::ModuleSizeInfo> captured;
    EXPECT_CALL(*printer, printModuleSizes(testing::_, testing::Gt(0U), testing::Gt(0U)))
      .WillRepeatedly(
        testing::Invoke([&captured](const std::vector<cli::Printer::ModuleSizeInfo> &sizes,
                                    std::uint64_t,
                                    std::uint64_t) {
            captured = sizes;
        }));

    EXPECT_EQ(cli->size(cli::SizeOptions{ .sortBy = "id", .ascending = true }), 0);
    ASSERT_EQ(captured.size(), 2U);
    EXPECT_EQ(captured.front().module, "binary");
    EXPECT_EQ(captured.back().module, "develop");
    EXPECT_GT(captured.front().actualSize, 0U);

    for (const auto &sortBy : { "logical", "exclusive", "shared", "actual" }) {
        for (bool ascending : { false, true }) {
            EXPECT_EQ(cli->size(cli::SizeOptions{ .sortBy = sortBy, .ascending = ascending }), 0)
              << "sortBy=" << sortBy << " ascending=" << ascending;
        }
    }
}

TEST_F(CliCommandTest, DependsBuildsTreeForImportedApplications)
{
    const auto repoRoot = tempDir->path() / "depends-repo";
    std::filesystem::create_directories(repoRoot);
    auto created = repo::OSTreeRepo::create(repoRoot, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto realRepo = std::move(*created);

    auto importInfo = [&](const v1::PackageInfoV2 &info,
                          const std::vector<std::string> &files) -> bool {
        const auto layerDir =
          tempDir->path() / ("depends-" + info.id + "-" + info.packageInfoV2Module);
        std::filesystem::create_directories(layerDir);
        for (const auto &file : files) {
            const auto path = layerDir / file;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path) << "content";
        }
        std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();
        package::LayerDir dir{ layerDir };
        return dir.valid() && realRepo->importLayerDir(dir).has_value();
    };

    auto baseInfo = makePackage("org.deepin.base", "23.0.0", "base", "binary");
    baseInfo.base = "org.deepin.base/23.0.0";

    auto runtimeInfo = makePackage("org.deepin.runtime", "23.0.0", "runtime", "binary");
    runtimeInfo.base = "org.deepin.base/23.0.0";

    auto appInfo = makePackage("org.example.depends", "1.0.0", "app", "binary");
    appInfo.base = "org.deepin.base/23.0.0";
    appInfo.runtime = "org.deepin.runtime/23.0.0";

    ASSERT_TRUE(importInfo(baseInfo, { "files/usr/bin/base" }));
    ASSERT_TRUE(importInfo(runtimeInfo, { "files/usr/lib/runtime.so" }));
    ASSERT_TRUE(importInfo(appInfo, { "files/usr/bin/depends" }));

    cli->setRepo(realRepo.get());

    std::vector<cli::Printer::DependsNode> captured;
    EXPECT_CALL(*printer, printDepends(testing::_))
      .WillOnce(testing::Invoke([&captured](const std::vector<cli::Printer::DependsNode> &trees) {
          captured = trees;
      }));

    ASSERT_EQ(cli->depends(cli::DependsOptions{}), 0);
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_THAT(captured.front().ref, testing::HasSubstr("org.deepin.base"));
    ASSERT_EQ(captured.front().children.size(), 1U);
    EXPECT_THAT(captured.front().children.front().ref, testing::HasSubstr("org.deepin.runtime"));
    ASSERT_EQ(captured.front().children.front().children.size(), 1U);
    EXPECT_THAT(captured.front().children.front().children.front().ref,
                testing::HasSubstr("org.example.depends"));
}

TEST_F(CliCommandTest, ListReportsImportedLayersAndFiltersByType)
{
    const auto repoRoot = tempDir->path() / "list-repo";
    std::filesystem::create_directories(repoRoot);
    auto created = repo::OSTreeRepo::create(repoRoot, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto realRepo = std::move(*created);

    auto importInfo = [&](const v1::PackageInfoV2 &info) -> bool {
        const auto layerDir = tempDir->path() / ("list-" + info.id);
        std::filesystem::create_directories(layerDir / "files");
        std::ofstream(layerDir / "files" / "payload") << "content";
        std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();
        package::LayerDir dir{ layerDir };
        return dir.valid() && realRepo->importLayerDir(dir).has_value();
    };

    ASSERT_TRUE(importInfo(makePackage("org.example.listapp", "1.0.0", "app", "binary")));
    ASSERT_TRUE(importInfo(makePackage("org.example.listbase", "23.0.0", "base", "binary")));

    cli->setRepo(realRepo.get());

    std::vector<v1::PackageInfoDisplay> captured;
    EXPECT_CALL(*printer, printPackages(testing::_))
      .WillRepeatedly(
        testing::Invoke([&captured](const std::vector<v1::PackageInfoDisplay> &packages) {
            captured = packages;
        }));

    ASSERT_EQ(cli->list(cli::ListOptions{ .type = "app" }), 0);
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured.front().id, "org.example.listapp");
    EXPECT_GT(captured.front().installTime, 0);

    ASSERT_EQ(cli->list(cli::ListOptions{ .type = "all" }), 0);
    EXPECT_EQ(captured.size(), 2U);
}

TEST_F(CliCommandTest, ListUpgradableReportsNewerRemoteVersions)
{
    const auto layerDir = tempDir->path() / "upgrade-app";
    std::filesystem::create_directories(layerDir / "files");
    std::ofstream(layerDir / "files" / "payload") << "content";
    auto info = makePackage("org.example.upgrade", "1.0.0", "app", "binary");
    std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();
    ASSERT_TRUE(package::LayerDir{ layerDir }.valid());
    ASSERT_TRUE(repoOwner->importLayerDir(package::LayerDir{ layerDir }).has_value());

    auto remoteRef = package::Reference::parse("stable:org.example.upgrade/2.0.0/x86_64");
    ASSERT_TRUE(remoteRef.has_value()) << remoteRef.error().message();
    const auto remote = package::ReferenceWithRepo{
        .repo = makeRepoConfig().repos.front(),
        .reference = *remoteRef,
    };
    ON_CALL(*repoOwner, latestRemoteReference(testing::_))
      .WillByDefault(testing::Invoke([remote](const package::FuzzyReference &)
                                       -> utils::error::Result<package::ReferenceWithRepo> {
          return remote;
      }));

    std::vector<v1::UpgradeListResult> captured;
    EXPECT_CALL(*printer, printUpgradeList(testing::_))
      .WillOnce(testing::Invoke([&captured](std::vector<v1::UpgradeListResult> &list) {
          captured = list;
      }));

    ASSERT_EQ(cli->list(cli::ListOptions{ .showUpgradeList = true }), 0);
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured.front().id, "org.example.upgrade");
    EXPECT_EQ(captured.front().oldVersion, "1.0.0");
    EXPECT_EQ(captured.front().newVersion, "2.0.0");
}

TEST_F(CliCommandTest, RunWithContextResolvesImportedLayersAndReportsRuntimeFailure)
{
    linglong::utils::EnvironmentVariableGuard runtimeDirGuard("XDG_RUNTIME_DIR",
                                                              tempDir->path().string());
    linglong::utils::EnvironmentVariableGuard homeGuard("HOME", tempDir->path().string());
    const auto repoRoot = tempDir->path() / "run-repo";
    std::filesystem::create_directories(repoRoot);
    auto created = repo::OSTreeRepo::create(repoRoot, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto realRepo = std::move(*created);

    auto importInfo = [&](const v1::PackageInfoV2 &info) -> bool {
        const auto layerDir = tempDir->path() / ("run-" + info.id);
        std::filesystem::create_directories(layerDir / "files");
        std::ofstream(layerDir / "files" / "payload") << "content";
        std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();
        package::LayerDir dir{ layerDir };
        return dir.valid() && realRepo->importLayerDir(dir).has_value();
    };

    auto baseInfo = makePackage("org.deepin.base", "23.0.0", "base", "binary");
    baseInfo.channel = "stable";
    auto runtimeInfo = makePackage("org.deepin.runtime", "23.0.0", "runtime", "binary");
    runtimeInfo.channel = "stable";
    runtimeInfo.base = "org.deepin.base/23.0.0";
    auto appInfo = makePackage("org.example.run", "1.0.0", "app", "binary");
    appInfo.channel = "stable";
    appInfo.base = "org.deepin.base/23.0.0";
    appInfo.runtime = "org.deepin.runtime/23.0.0";
    appInfo.command = std::vector<std::string>{ "echo", "hi" };

    ASSERT_TRUE(importInfo(baseInfo));
    ASSERT_TRUE(importInfo(runtimeInfo));
    ASSERT_TRUE(importInfo(appInfo));

    auto merged = realRepo->mergeModules();
    ASSERT_TRUE(merged.has_value()) << merged.error().message();

    cli->setRepo(realRepo.get());

    auto config = v1::RunContextConfig{
        .app = "stable:org.example.run/1.0.0/x86_64",
        .base = "stable:org.deepin.base/23.0.0/x86_64",
        .runtime = "stable:org.deepin.runtime/23.0.0/x86_64",
        .version = "1",
    };

    EXPECT_CALL(*printer, printErr(testing::_)).Times(testing::AtLeast(1));

    cli::RunOptions options;
    options.appid = "org.example.run";
    options.runContext = nlohmann::json(config).dump();
    options.filePaths = { tempDir->path().string() };
    options.fileUrls = { "file://" + tempDir->path().string() };
    options.commands = { "echo", "hi" };
    options.disableXdp = true;

    EXPECT_EQ(cli->runWithContext(options), -1);
}

TEST_F(CliCommandTest, RunPreparesImportedApplicationAndReportsCacheFailure)
{
    test::ScopedRepoLockFile repoLockFile;
    if (!repoLockFile.ok()) {
        GTEST_SKIP() << repoLockFile.error();
    }

    linglong::utils::EnvironmentVariableGuard runtimeDirGuard("XDG_RUNTIME_DIR",
                                                              tempDir->path().string());
    linglong::utils::EnvironmentVariableGuard homeGuard("HOME", tempDir->path().string());
    const auto repoRoot = tempDir->path() / "run-cache-repo";
    std::filesystem::create_directories(repoRoot);
    auto created = repo::OSTreeRepo::create(repoRoot, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto realRepo = std::move(*created);

    auto importInfo = [&](const v1::PackageInfoV2 &info) -> bool {
        const auto layerDir = tempDir->path() / ("run-cache-" + info.id);
        std::filesystem::create_directories(layerDir / "files");
        std::ofstream(layerDir / "files" / "payload") << "content";
        std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();
        package::LayerDir dir{ layerDir };
        return dir.valid() && realRepo->importLayerDir(dir).has_value();
    };

    auto baseInfo = makePackage("org.deepin.base", "23.0.0", "base", "binary");
    baseInfo.channel = "stable";
    auto runtimeInfo = makePackage("org.deepin.runtime", "23.0.0", "runtime", "binary");
    runtimeInfo.channel = "stable";
    runtimeInfo.base = "org.deepin.base/23.0.0";
    auto appInfo = makePackage("org.example.cache", "1.0.0", "app", "binary");
    appInfo.channel = "stable";
    appInfo.base = "org.deepin.base/23.0.0";
    appInfo.runtime = "org.deepin.runtime/23.0.0";
    appInfo.command = std::vector<std::string>{ "echo", "hi" };

    ASSERT_TRUE(importInfo(baseInfo));
    ASSERT_TRUE(importInfo(runtimeInfo));
    ASSERT_TRUE(importInfo(appInfo));

    auto merged = realRepo->mergeModules();
    ASSERT_TRUE(merged.has_value()) << merged.error().message();
    cli->setRepo(realRepo.get());

    useError(static_cast<int>(utils::error::ErrorCode::AppInstallFailed), "cache unavailable");
    EXPECT_CALL(*printer, printErr(testing::_)).Times(testing::AtLeast(1));

    cli::RunOptions options;
    options.appid = "org.example.cache";
    options.commands = { "echo", "hi" };
    options.disableXdp = true;

    EXPECT_EQ(cli->run(options), -1);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("InitRunContext"));
}

TEST_F(CliCommandTest, InstallWithoutExplicitModuleBuildsLanguageModules)
{
    publishTask(commonResult("installed"));

    linglong::utils::EnvironmentVariableGuard langGuard("LANG", "zh_CN.UTF-8");
    linglong::utils::EnvironmentVariableGuard allGuard("LC_ALL", "en");
    linglong::utils::EnvironmentVariableGuard timeGuard("LC_TIME", "de_DE");
    linglong::utils::EnvironmentVariableGuard numericGuard("LC_NUMERIC", "12_34");
    linglong::utils::EnvironmentVariableGuard nameGuard("LC_NAME", "f");
    linglong::utils::EnvironmentVariableGuard paperGuard("LC_PAPER", "fr@euro");
    linglong::utils::EnvironmentVariableGuard phoneGuard("LC_TELEPHONE", "fr_FR.UTF-8");

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App" }), 0);
    EXPECT_EQ(pkgManService->lastMethod, QStringLiteral("Install"));

    const auto packageArgument = pkgManService->lastParams.value("package");
    ASSERT_TRUE(packageArgument.canConvert<QDBusArgument>());
    const auto package = qdbus_cast<QVariantMap>(packageArgument.value<QDBusArgument>());
    const auto modulesArgument = package.value("modules");
    QStringList moduleNames;
    if (modulesArgument.canConvert<QDBusArgument>()) {
        const auto argument = modulesArgument.value<QDBusArgument>();
        const auto modules = qdbus_cast<QVariantList>(argument);
        for (const auto &module : modules) {
            moduleNames.push_back(module.toString());
        }
        if (moduleNames.isEmpty()) {
            moduleNames = qdbus_cast<QStringList>(argument);
        }
    } else {
        for (const auto &module : modulesArgument.toList()) {
            moduleNames.push_back(module.toString());
        }
    }

    const auto joined = moduleNames.join(",").toStdString();
    EXPECT_TRUE(moduleNames.contains("binary")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_zh")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_zh_CN")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_en")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_de")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_de_DE")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_fr")) << joined;
    EXPECT_TRUE(moduleNames.contains("lang_fr_FR")) << joined;
}

namespace {

std::string makeContainerListJson(const std::vector<std::pair<std::string, int64_t>> &items)
{
    std::string out = "[";
    bool first = true;
    for (const auto &[id, pid] : items) {
        if (!first) {
            out += ",";
        }
        first = false;
        out += R"({"bundle":"/tmp/bundle","created":"2026-01-01T00:00:00Z","id":")" + id
          + R"(","owner":"root","pid":)" + std::to_string(pid) + R"(,"status":"running"})";
    }
    out += "]";
    return out;
}

std::filesystem::path makeFakeCrun(const std::filesystem::path &dir,
                                   const std::string &listJson,
                                   int killExit = 0,
                                   int execExit = 0)
{
    static std::atomic<int> scriptCounter{ 0 };
    auto path =
      dir / ("fake-crun-" + std::to_string(::getpid()) + "-" + std::to_string(++scriptCounter));
    std::ofstream script(path);
    script << "#!/bin/sh\n";
    script << "for arg in \"$@\"; do\n";
    script << "  if [ \"$arg\" = \"list\" ]; then\n";
    script << "    cat <<'JSONEOF'\n" << listJson << "\nJSONEOF\n";
    script << "    exit 0\n";
    script << "  fi\n";
    script << "  if [ \"$arg\" = \"kill\" ]; then exit " << killExit << "; fi\n";
    script << "  if [ \"$arg\" = \"exec\" ]; then exit " << execExit << "; fi\n";
    script << "  if [ \"$arg\" = \"run\" ]; then exit " << execExit << "; fi\n";
    script << "done\n";
    script << "exit 0\n";
    script.close();
    std::error_code ec;
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_all
                                   | std::filesystem::perms::group_exec
                                   | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace,
                                 ec);
    EXPECT_FALSE(ec) << ec.message();
    return path;
}

class RuntimeStateDir
{
public:
    RuntimeStateDir()
        : dir(std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid()))
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        ok = !ec;
        error = ec.message();
    }

    ~RuntimeStateDir()
    {
        for (const auto &file : created) {
            std::error_code ec;
            std::filesystem::remove(file, ec);
        }
        if (createdParent) {
            std::error_code ec;
            std::filesystem::remove(dir, ec);
        }
    }

    bool writeState(const std::string &name, const v1::ContainerProcessStateInfo &info)
    {
        auto path = dir / name;
        std::ofstream out(path);
        out << nlohmann::json(info).dump();
        out.close();
        if (!out) {
            return false;
        }
        created.push_back(path);
        return true;
    }

    bool good() const { return ok; }

    const std::string &errorMessage() const { return error; }

    const std::filesystem::path &path() const { return dir; }

private:
    std::filesystem::path dir;
    std::vector<std::filesystem::path> created;
    bool ok{ false };
    bool createdParent{ false };
    std::string error;
};

struct ContainerCli
{
    std::unique_ptr<ocppi::cli::crun::Crun> oci;
    std::unique_ptr<runtime::ContainerBuilder> builder;
    std::unique_ptr<testing::NiceMock<MockNotifier>> notifier;
    std::unique_ptr<testing::NiceMock<MockPrinter>> printer;
    std::unique_ptr<TestCli> cli;
};

ContainerCli makeContainerCli(const std::filesystem::path &crunPath)
{
    ContainerCli result;
    result.oci = ocppi::cli::crun::Crun::New(crunPath).value();
    result.builder = std::make_unique<runtime::ContainerBuilder>(*result.oci);
    result.notifier = std::make_unique<testing::NiceMock<MockNotifier>>();
    result.printer = std::make_unique<testing::NiceMock<MockPrinter>>();
    result.cli = std::make_unique<TestCli>(*result.printer,
                                           *result.oci,
                                           *result.builder,
                                           false,
                                           std::move(result.notifier),
                                           nullptr);
    return result;
}

} // namespace

TEST_F(CliCommandTest, PsReportsRuntimeEnumerationFailure)
{
    EXPECT_CALL(*printer, printErr(::testing::_));

    EXPECT_EQ(cli->ps(cli::PsOptions{}), -1);
}

TEST_F(CliCommandTest, PsPrintsRunningContainersTruncatedAndFull)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "0123456789abcdef";
    v1::ContainerProcessStateInfo info{ .app = "main:org.example.ps/1.0.0/x86_64",
                                        .base = "main:org.example.base/1.0.0/x86_64",
                                        .containerID = containerId };
    ASSERT_TRUE(stateDir.writeState(std::to_string(::getpid()), info));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 4321 } }));
    auto containerCli = makeContainerCli(crun);

    std::vector<v1::CliContainer> truncated;
    EXPECT_CALL(*containerCli.printer, printContainers(::testing::_))
      .WillOnce(::testing::Invoke([&truncated](const std::vector<v1::CliContainer> &containers) {
          truncated = containers;
      }));

    ASSERT_EQ(containerCli.cli->ps(cli::PsOptions{}), 0);
    ASSERT_EQ(truncated.size(), 1U);
    EXPECT_EQ(truncated.front().id, containerId.substr(0, 12));
    EXPECT_EQ(truncated.front().pid, 4321);

    std::vector<v1::CliContainer> full;
    EXPECT_CALL(*containerCli.printer, printContainers(::testing::_))
      .WillOnce(::testing::Invoke([&full](const std::vector<v1::CliContainer> &containers) {
          full = containers;
      }));

    ASSERT_EQ(containerCli.cli->ps(cli::PsOptions{ .noTruncate = true }), 0);
    ASSERT_EQ(full.size(), 1U);
    EXPECT_EQ(full.front().id, containerId);
}

TEST_F(CliCommandTest, PsSkipsStaleAndMalformedContainerState)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    // The PID does not exist, so this state file must be ignored.
    ASSERT_TRUE(stateDir.writeState(
      "999999999",
      v1::ContainerProcessStateInfo{ .app = "main:org.example.stale/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = "stale-container" }));
    // Malformed JSON must be ignored instead of aborting enumeration.
    {
        auto path = stateDir.path() / "424242";
        std::ofstream(path) << "{not json";
        std::error_code ec;
        std::filesystem::resize_file(path, 8, ec);
    }

    auto crun = makeFakeCrun(tempDir->path(), "[]");
    auto containerCli = makeContainerCli(crun);

    std::vector<v1::CliContainer> captured{ { .id = "sentinel" } };
    EXPECT_CALL(*containerCli.printer, printContainers(::testing::_))
      .WillOnce(::testing::Invoke([&captured](const std::vector<v1::CliContainer> &containers) {
          captured = containers;
      }));

    ASSERT_EQ(containerCli.cli->ps(cli::PsOptions{}), 0);
    EXPECT_TRUE(captured.empty());

    std::error_code ec;
    std::filesystem::remove(stateDir.path() / "424242", ec);
}

TEST_F(CliCommandTest, KillTerminatesContainerMatchedByPackageId)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "aaaabbbbccccdddd";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.kill/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = containerId }));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 11 } }));
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_)).Times(0);
    EXPECT_EQ(containerCli.cli->kill(cli::KillOptions{ .appid = "org.example.kill" }), 0);
}

TEST_F(CliCommandTest, KillReportsNoMatchingContainer)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    auto crun = makeFakeCrun(tempDir->path(), "[]");
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_));
    EXPECT_EQ(containerCli.cli->kill(cli::KillOptions{ .appid = "org.example.missing" }), -1);
}

TEST_F(CliCommandTest, KillReportsRuntimeFailure)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "ddddeeeeffffgggg";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.killfail/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = containerId }));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 12 } }), 1, 0);
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_));
    EXPECT_EQ(containerCli.cli->kill(cli::KillOptions{ .appid = "org.example.killfail" }), -1);
}

TEST_F(CliCommandTest, KillRejectsAmbiguousApplicationMatch)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string firstId = "1111222233334444";
    const std::string secondId = "5555666677778888";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.ambiguous/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = firstId }));
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getppid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.ambiguous/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = secondId }));

    auto crun =
      makeFakeCrun(tempDir->path(), makeContainerListJson({ { firstId, 21 }, { secondId, 22 } }));
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_));
    EXPECT_EQ(containerCli.cli->kill(cli::KillOptions{ .appid = "org.example.ambiguous" }), -1);
}

TEST_F(CliCommandTest, EnterRunsCommandInContainerMatchedByShortId)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "99990000aaaabbbb";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.enter/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = containerId }));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 31 } }));
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_)).Times(0);
    EXPECT_EQ(containerCli.cli->enter(cli::EnterOptions{ .instance = containerId.substr(0, 12),
                                                         .commands = { "echo", "hi" } }),
              0);
}

TEST_F(CliCommandTest, EnterReportsMissingContainer)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    auto crun = makeFakeCrun(tempDir->path(), "[]");
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_));
    EXPECT_EQ(containerCli.cli->enter(cli::EnterOptions{ .instance = "org.example.missing" }), -1);
}

TEST_F(CliCommandTest, EnterReturnsChildExitStatus)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "ccccddddeeeeffff";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.enterfail/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = containerId }));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 41 } }), 0, 7);
    auto containerCli = makeContainerCli(crun);

    EXPECT_CALL(*containerCli.printer, printErr(::testing::_));
    EXPECT_EQ(containerCli.cli->enter(cli::EnterOptions{ .instance = "org.example.enterfail" }), 7);
}

TEST_F(CliCommandTest, InspectDispatchesDirBundleAndRemoteInfo)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    RuntimeStateDir stateDir;
    ASSERT_TRUE(stateDir.good()) << stateDir.errorMessage();

    const std::string containerId = "ffffeeeeddddcccc";
    ASSERT_TRUE(stateDir.writeState(
      std::to_string(::getpid()),
      v1::ContainerProcessStateInfo{ .app = "main:org.example.bundle/1.0.0/x86_64",
                                     .base = "main:org.example.base/1.0.0/x86_64",
                                     .containerID = containerId }));

    auto crun = makeFakeCrun(tempDir->path(), makeContainerListJson({ { containerId, 51 } }));
    auto containerCli = makeContainerCli(crun);

    CLI::App app;
    app.add_subcommand("dir");
    app.add_subcommand("remote-info");
    app.parse(std::vector<std::string>{ "dir" });

    EXPECT_EQ(containerCli.cli->inspect(
                &app,
                cli::InspectOptions{ .appid = "org.example.bundle", .dirType = "bundle" }),
              0);
}

TEST_F(CliCommandTest, RunReportsMissingLocalApplication)
{
    if (!linglong::test::isIsolatedRuntime()) {
        GTEST_SKIP() << linglong::test::isolatedRuntimeRequirement();
    }

    const auto userDir = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());
    std::filesystem::create_directories(userDir);

    ON_CALL(*repoOwner, clearReferenceLocal(::testing::_, ::testing::_))
      .WillByDefault(::testing::Invoke([](const package::FuzzyReference &, bool) {
          LINGLONG_TRACE("clearReferenceLocal");
          return LINGLONG_ERR("package not found");
      }));

    EXPECT_CALL(*printer, printErr(::testing::_));
    EXPECT_EQ(cli->run(cli::RunOptions{ .appid = "org.example.not-installed" }), -1);

    std::error_code ec;
    std::filesystem::remove(userDir, ec);
}

} // namespace

#include "cli_command_test.moc"
