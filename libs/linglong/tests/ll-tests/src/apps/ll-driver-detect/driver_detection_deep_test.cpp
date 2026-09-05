// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "application_singleton.h"
#include "dbus_notifier.h"
#include "driver_detection_config.h"
#include "driver_detection_manager.h"
#include "driver_detector.h"
#include "nvidia_driver_detector.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

namespace linglong::driver::detect::test {

namespace fs = std::filesystem;
using namespace testing;

class MockDriverDetectorFull : public DriverDetector
{
public:
    MOCK_METHOD(linglong::utils::error::Result<GraphicsDriverInfo>, detect, (), (override));
    MOCK_METHOD(std::string, getDriverIdentify, (), (const, override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageInstalled,
                (const std::string &packageName),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageUpgradable,
                (const std::string &packageName),
                (override));
};

class TestableNvidiaDetector : public NVIDIADriverDetector
{
public:
    using NVIDIADriverDetector::NVIDIADriverDetector;

    MOCK_METHOD((linglong::utils::error::Result<std::pair<bool, GraphicsDriverInfo>>),
                getPackageInfoFromRemoteRepo,
                (const std::string &packageName),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageInstalled,
                (const std::string &packageName),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageUpgradable,
                (const std::string &packageName),
                (override));
};

class DriverDetectionManagerDeepSuite : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(DriverDetectionManagerDeepSuite, DetectMultipleHardwareVendorsSuccess)
{
    auto nvidiaDetector = std::make_unique<MockDriverDetectorFull>();
    auto amdDetector = std::make_unique<MockDriverDetectorFull>();
    auto intelDetector = std::make_unique<MockDriverDetectorFull>();

    EXPECT_CALL(*nvidiaDetector, detect())
      .WillOnce(Return(linglong::utils::error::Result<GraphicsDriverInfo>(
        GraphicsDriverInfo{ "nvidia", "org.deepin.nvidia.driver", "550.54.14", "stable" })));

    EXPECT_CALL(*amdDetector, detect())
      .WillOnce(Return(linglong::utils::error::Result<GraphicsDriverInfo>(
        GraphicsDriverInfo{ "amd", "org.deepin.amdgpu.pro", "23.40.2", "stable" })));

    EXPECT_CALL(*intelDetector, detect())
      .WillOnce(Return(LINGLONG_ERR("No compatible Intel Arc hardware found")));
    EXPECT_CALL(*intelDetector, getDriverIdentify()).WillOnce(Return("intel"));

    std::vector<std::unique_ptr<DriverDetector>> detectors;
    detectors.push_back(std::move(nvidiaDetector));
    detectors.push_back(std::move(amdDetector));
    detectors.push_back(std::move(intelDetector));

    DriverDetectionManager manager(std::move(detectors));
    auto driversRes = manager.detectAvailableDrivers();

    ASSERT_TRUE(driversRes.has_value());
    ASSERT_EQ(driversRes->size(), 2);
    EXPECT_EQ((*driversRes)[0].identify, "nvidia");
    EXPECT_EQ((*driversRes)[0].packageName, "org.deepin.nvidia.driver");
    EXPECT_EQ((*driversRes)[1].identify, "amd");
    EXPECT_EQ((*driversRes)[1].packageName, "org.deepin.amdgpu.pro");
}

TEST_F(DriverDetectionManagerDeepSuite, EmptyDriversDetectionReturnsEmptyVector)
{
    std::vector<std::unique_ptr<DriverDetector>> emptyDetectors;
    DriverDetectionManager manager(std::move(emptyDetectors));

    auto driversRes = manager.detectAvailableDrivers();
    ASSERT_TRUE(driversRes.has_value());
    EXPECT_TRUE(driversRes->empty());
}

TEST_F(DriverDetectionManagerDeepSuite, InstallDriverPackageWithEmptyListIsNoOp)
{
    std::vector<std::unique_ptr<DriverDetector>> detectors;
    DriverDetectionManager manager(std::move(detectors));

    std::vector<GraphicsDriverInfo> emptyDrivers;
    auto installRes = manager.installDriverPackage(emptyDrivers);
    EXPECT_TRUE(installRes.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, ConfigManagerFileOperations)
{
    auto cfgPath = tempDir.path() / "driver_detect_cfg.ini";
    DriverDetectionConfigManager configMgr(cfgPath.string());

    EXPECT_TRUE(configMgr.shouldShowNotification());
    EXPECT_FALSE(configMgr.getConfig().neverRemind);

    ASSERT_TRUE(configMgr.saveConfig());
    EXPECT_TRUE(fs::exists(cfgPath));

    DriverDetectionConfig newCfg{ .neverRemind = true };
    configMgr.setConfig(newCfg);
    EXPECT_FALSE(configMgr.shouldShowNotification());
    ASSERT_TRUE(configMgr.saveConfig());

    DriverDetectionConfigManager reloadedMgr(cfgPath.string());
    ASSERT_TRUE(reloadedMgr.loadConfig());
    EXPECT_TRUE(reloadedMgr.getConfig().neverRemind);
    EXPECT_FALSE(reloadedMgr.shouldShowNotification());

    reloadedMgr.recordUserChoice(UserNotificationChoice::InstallNow);
    EXPECT_FALSE(reloadedMgr.getConfig().neverRemind);
    EXPECT_TRUE(reloadedMgr.shouldShowNotification());

    reloadedMgr.recordUserChoice(UserNotificationChoice::NeverRemind);
    EXPECT_TRUE(reloadedMgr.getConfig().neverRemind);
    EXPECT_FALSE(reloadedMgr.shouldShowNotification());
}

TEST_F(DriverDetectionManagerDeepSuite, ConfigManagerInvalidFilePaths)
{
    auto invalidPath = tempDir.path() / "not_exist_subdir" / "driver_cfg.ini";
    DriverDetectionConfigManager invalidMgr(invalidPath.string());

    EXPECT_FALSE(invalidMgr.loadConfig());

    auto nonDirectoryPath = tempDir.path() / "plain_file.txt";
    {
        std::ofstream ofs(nonDirectoryPath);
        ofs << "not a directory";
    }
    auto impossibleSubdir = nonDirectoryPath / "cannot_create" / "cfg.ini";
    DriverDetectionConfigManager impossibleMgr(impossibleSubdir.string());
    EXPECT_FALSE(impossibleMgr.saveConfig());
}

TEST_F(DriverDetectionManagerDeepSuite, ApplicationSingletonLockLifecycle)
{
    auto lockPath = tempDir.path() / "app_driver.lock";

    {
        ApplicationSingleton singleton1(lockPath.string());
        EXPECT_FALSE(singleton1.isLockHeld());

        auto acquire1 = singleton1.tryAcquireLock();
        ASSERT_TRUE(acquire1.has_value());
        EXPECT_TRUE(*acquire1);
        EXPECT_TRUE(singleton1.isLockHeld());

        ApplicationSingleton singleton2(lockPath.string());
        auto acquire2 = singleton2.tryAcquireLock();
        ASSERT_TRUE(acquire2.has_value());
        EXPECT_FALSE(*acquire2);
        EXPECT_FALSE(singleton2.isLockHeld());

        singleton1.releaseLock();
        EXPECT_FALSE(singleton1.isLockHeld());

        auto acquire3 = singleton2.tryAcquireLock();
        ASSERT_TRUE(acquire3.has_value());
        EXPECT_TRUE(*acquire3);
        EXPECT_TRUE(singleton2.isLockHeld());
    }

    ApplicationSingleton singleton3(lockPath.string());
    auto acquire4 = singleton3.tryAcquireLock();
    ASSERT_TRUE(acquire4.has_value());
    EXPECT_TRUE(*acquire4);
    EXPECT_TRUE(singleton3.isLockHeld());
}

TEST_F(DriverDetectionManagerDeepSuite, ApplicationSingletonConcurrentThreads)
{
    auto lockPath = tempDir.path() / "concurrent.lock";
    std::atomic<int> successCount{ 0 };

    auto worker = [&]() {
        ApplicationSingleton s(lockPath.string());
        auto res = s.tryAcquireLock();
        if (res.has_value() && *res) {
            successCount.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            s.releaseLock();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    EXPECT_GE(successCount.load(), 1);
}

TEST_F(DriverDetectionManagerDeepSuite, GraphicsDriverInfoFieldsAssignment)
{
    GraphicsDriverInfo info1;
    info1.identify = "nvidia-proprietary";
    info1.packageName = "com.nvidia.driver";
    info1.packageVersion = "535.183.01";
    info1.repoName = "official";

    EXPECT_EQ(info1.identify, "nvidia-proprietary");
    EXPECT_EQ(info1.packageName, "com.nvidia.driver");
    EXPECT_EQ(info1.packageVersion, "535.183.01");
    EXPECT_EQ(info1.repoName, "official");

    GraphicsDriverInfo info2{
        .identify = "mesa-radv",
        .packageName = "org.freedesktop.mesa",
        .packageVersion = "24.0.0",
        .repoName = "stable",
    };
    EXPECT_EQ(info2.identify, "mesa-radv");
    EXPECT_EQ(info2.repoName, "stable");
}

TEST_F(DriverDetectionManagerDeepSuite, CheckPackageInstalledAndUpgradableMocks)
{
    auto mockDetector = std::make_unique<MockDriverDetectorFull>();

    EXPECT_CALL(*mockDetector, checkPackageInstalled("nvidia-open-kernel"))
      .WillOnce(Return(linglong::utils::error::Result<bool>(true)))
      .WillOnce(Return(linglong::utils::error::Result<bool>(false)));

    EXPECT_CALL(*mockDetector, checkPackageUpgradable("nvidia-open-kernel"))
      .WillOnce(Return(linglong::utils::error::Result<bool>(true)))
      .WillOnce(Return(LINGLONG_ERR("Remote repository unreachable")));

    auto resInst1 = mockDetector->checkPackageInstalled("nvidia-open-kernel");
    ASSERT_TRUE(resInst1.has_value());
    EXPECT_TRUE(*resInst1);

    auto resInst2 = mockDetector->checkPackageInstalled("nvidia-open-kernel");
    ASSERT_TRUE(resInst2.has_value());
    EXPECT_FALSE(*resInst2);

    auto resUpg1 = mockDetector->checkPackageUpgradable("nvidia-open-kernel");
    ASSERT_TRUE(resUpg1.has_value());
    EXPECT_TRUE(*resUpg1);

    auto resUpg2 = mockDetector->checkPackageUpgradable("nvidia-open-kernel");
    EXPECT_FALSE(resUpg2.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorVersionFileParsing)
{
    auto fakeVersionFile = tempDir.path() / "nvidia_version";
    {
        std::ofstream ofs(fakeVersionFile);
        ofs << "550.54.14\n";
    }

    NVIDIADriverDetector detector(fakeVersionFile.string());
    EXPECT_EQ(detector.getDriverIdentify(), NVIDIADriverDetector::kNvidiaPackageIdentify);

    auto nonExistentDetector = NVIDIADriverDetector((tempDir.path() / "missing_file").string());
    auto detectNone = nonExistentDetector.detect();
    EXPECT_FALSE(detectNone.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorVersionEmptyOrWhitespace)
{
    auto emptyVersionFile = tempDir.path() / "empty_version";
    {
        std::ofstream ofs(emptyVersionFile);
        ofs << "   \n\t  \n";
    }

    NVIDIADriverDetector detector(emptyVersionFile.string());
    auto detectRes = detector.detect();
    EXPECT_FALSE(detectRes.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorBranchesNotInstalledRemoteExists)
{
    auto fakeVersionFile = tempDir.path() / "nv_ver1";
    {
        std::ofstream ofs(fakeVersionFile);
        ofs << "550.54.14\n";
    }

    TestableNvidiaDetector detector(fakeVersionFile.string());
    std::string expectedPkg =
      std::string(NVIDIADriverDetector::kNvidiaPackageIdentify) + ".550.54.14";

    GraphicsDriverInfo remoteInfo{
        .identify = NVIDIADriverDetector::kNvidiaPackageIdentify,
        .packageName = expectedPkg,
        .packageVersion = "550.54.14",
        .repoName = "stable",
    };

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPkg))
      .WillOnce(Return(std::make_pair(true, remoteInfo)));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPkg)).WillOnce(Return(false));

    auto detectRes = detector.detect();
    ASSERT_TRUE(detectRes.has_value());
    EXPECT_EQ(detectRes->packageName, expectedPkg);
    EXPECT_EQ(detectRes->packageVersion, "550.54.14");
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorBranchesInstalledAndUpgradable)
{
    auto fakeVersionFile = tempDir.path() / "nv_ver2";
    {
        std::ofstream ofs(fakeVersionFile);
        ofs << "550.54.14\n";
    }

    TestableNvidiaDetector detector(fakeVersionFile.string());
    std::string expectedPkg =
      std::string(NVIDIADriverDetector::kNvidiaPackageIdentify) + ".550.54.14";

    GraphicsDriverInfo remoteInfo{
        .identify = NVIDIADriverDetector::kNvidiaPackageIdentify,
        .packageName = expectedPkg,
        .packageVersion = "550.54.15",
        .repoName = "stable",
    };

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPkg))
      .WillOnce(Return(std::make_pair(true, remoteInfo)));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPkg)).WillOnce(Return(true));

    EXPECT_CALL(detector, checkPackageUpgradable(expectedPkg)).WillOnce(Return(true));

    auto detectRes = detector.detect();
    ASSERT_TRUE(detectRes.has_value());
    EXPECT_EQ(detectRes->packageVersion, "550.54.15");
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorBranchesInstalledAlreadyLatest)
{
    auto fakeVersionFile = tempDir.path() / "nv_ver3";
    {
        std::ofstream ofs(fakeVersionFile);
        ofs << "550.54.14\n";
    }

    TestableNvidiaDetector detector(fakeVersionFile.string());
    std::string expectedPkg =
      std::string(NVIDIADriverDetector::kNvidiaPackageIdentify) + ".550.54.14";

    GraphicsDriverInfo remoteInfo{
        .identify = NVIDIADriverDetector::kNvidiaPackageIdentify,
        .packageName = expectedPkg,
        .packageVersion = "550.54.14",
        .repoName = "stable",
    };

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPkg))
      .WillOnce(Return(std::make_pair(true, remoteInfo)));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPkg)).WillOnce(Return(true));

    EXPECT_CALL(detector, checkPackageUpgradable(expectedPkg)).WillOnce(Return(false));

    auto detectRes = detector.detect();
    EXPECT_FALSE(detectRes.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, NvidiaDetectorBranchesRemoteLookupFailure)
{
    auto fakeVersionFile = tempDir.path() / "nv_ver4";
    {
        std::ofstream ofs(fakeVersionFile);
        ofs << "550.54.14\n";
    }

    TestableNvidiaDetector detector(fakeVersionFile.string());
    std::string expectedPkg =
      std::string(NVIDIADriverDetector::kNvidiaPackageIdentify) + ".550.54.14";

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPkg))
      .WillOnce(Return(LINGLONG_ERR("Remote repository connection timeout")));

    auto detectRes = detector.detect();
    EXPECT_FALSE(detectRes.has_value());
}

TEST_F(DriverDetectionManagerDeepSuite, DBusNotifierStructsInitialization)
{
    DBusNotifier::NotificationRequest req;
    req.summary = "Graphics driver update";
    req.body = "A new NVIDIA display driver (550.54.14) is available.";
    req.appName = "linglong-driver-detector";
    req.icon = "video-display";
    req.timeout = 5000;
    req.actions = { "install", "Install Now", "dismiss", "Not Now" };

    EXPECT_EQ(req.summary, "Graphics driver update");
    EXPECT_EQ(req.appName, "linglong-driver-detector");
    EXPECT_EQ(req.actions.size(), 4);
    EXPECT_EQ(req.timeout, 5000u);

    DBusNotifier::NotificationResult resDef;
    EXPECT_FALSE(resDef.success);
    EXPECT_TRUE(resDef.action.isEmpty());

    DBusNotifier::NotificationResult resCustom("install", true);
    EXPECT_TRUE(resCustom.success);
    EXPECT_EQ(resCustom.action, "install");
}

TEST_F(DriverDetectionManagerDeepSuite, MultipleDetectorsMixedResults)
{
    auto detector1 = std::make_unique<MockDriverDetectorFull>();
    auto detector2 = std::make_unique<MockDriverDetectorFull>();
    auto detector3 = std::make_unique<MockDriverDetectorFull>();

    EXPECT_CALL(*detector1, detect())
      .WillOnce(Return(linglong::utils::error::Result<GraphicsDriverInfo>(
        GraphicsDriverInfo{ "nvidia", "driver.nvidia", "535.120", "repo1" })));

    EXPECT_CALL(*detector2, detect())
      .WillOnce(Return(LINGLONG_ERR("Device busy or driver in use")));
    EXPECT_CALL(*detector2, getDriverIdentify()).WillOnce(Return("generic-gpu"));

    EXPECT_CALL(*detector3, detect())
      .WillOnce(Return(linglong::utils::error::Result<GraphicsDriverInfo>(
        GraphicsDriverInfo{ "mesa", "driver.mesa", "23.3.0", "repo2" })));

    std::vector<std::unique_ptr<DriverDetector>> detectors;
    detectors.push_back(std::move(detector1));
    detectors.push_back(std::move(detector2));
    detectors.push_back(std::move(detector3));

    DriverDetectionManager manager(std::move(detectors));
    auto res = manager.detectAvailableDrivers();

    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 2);
    EXPECT_EQ((*res)[0].identify, "nvidia");
    EXPECT_EQ((*res)[1].identify, "mesa");
}

TEST_F(DriverDetectionManagerDeepSuite, DriverDetectorDestructorChainAndPolymorphism)
{
    std::vector<std::unique_ptr<DriverDetector>> detectors;

    auto d1 = std::make_unique<MockDriverDetectorFull>();
    EXPECT_CALL(*d1, getDriverIdentify()).WillRepeatedly(Return("driver1"));
    EXPECT_CALL(*d1, detect()).WillRepeatedly(Return(LINGLONG_ERR("No hardware")));

    auto d2 = std::make_unique<MockDriverDetectorFull>();
    EXPECT_CALL(*d2, getDriverIdentify()).WillRepeatedly(Return("driver2"));
    EXPECT_CALL(*d2, detect()).WillRepeatedly(Return(LINGLONG_ERR("Disabled")));

    detectors.push_back(std::move(d1));
    detectors.push_back(std::move(d2));

    {
        DriverDetectionManager mgr(std::move(detectors));
        auto detected = mgr.detectAvailableDrivers();
        ASSERT_TRUE(detected.has_value());
        EXPECT_TRUE(detected->empty());
    }
}

TEST_F(DriverDetectionManagerDeepSuite, UserNotificationChoiceEnumValues)
{
    auto choice1 = UserNotificationChoice::InstallNow;
    auto choice2 = UserNotificationChoice::NeverRemind;

    EXPECT_NE(choice1, choice2);
    EXPECT_EQ(choice1, UserNotificationChoice::InstallNow);
    EXPECT_EQ(choice2, UserNotificationChoice::NeverRemind);
}

} // namespace linglong::driver::detect::test
