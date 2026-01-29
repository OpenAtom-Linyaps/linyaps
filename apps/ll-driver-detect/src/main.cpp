// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "application_singleton.h"
#include "configure.h"
#include "dbus_notifier.h"
#include "driver_detection_config.h"
#include "driver_detection_manager.h"
#include "driver_detector.h"
#include "linglong/common/global/initialize.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/gettext.h"
#include "tl/expected.hpp"

#include <CLI/CLI.hpp>

#include <QCoreApplication>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using namespace linglong::driver::detect;

constexpr auto kActionInstallNow = "install_now";
constexpr auto kActionNotRemind = "not_remind";
constexpr auto kNotificationIcon = "/usr/share/icons/hicolor/scalable/apps/linyaps.svg";

struct DriverDetectOptions
{
    std::string driverType;
    std::string packageName;
    bool force = false;
    bool checkOnly = false;
    bool installOnly = false;
    int notificationIntervalMinutes = 5; // Kept for backward compatibility, but will be ignored
};

linglong::utils::error::Result<std::filesystem::path> getDefaultConfigPath()
{
    LINGLONG_TRACE("get default config path");

    auto *homeEnv = ::getenv("HOME");
    if (homeEnv == nullptr) {
        return LINGLONG_ERR("Couldn't get HOME env.");
    }

    auto value = getenv("XDG_CONFIG_HOME");
    auto configDir =
      value ? std::filesystem::path{ value } : std::filesystem::path{ homeEnv } / ".config";
    return configDir / "linglong/driver_detection.json";
}

// Sets up the configuration manager based on command-line options.
linglong::utils::error::Result<DriverDetectionConfigManager> setupConfigManager()
{
    LINGLONG_TRACE("setupConfigManager");

    auto defaultConfigPathResult = getDefaultConfigPath();
    if (!defaultConfigPathResult) {
        return LINGLONG_ERR(defaultConfigPathResult.error());
    }

    // Determine configuration path
    std::filesystem::path configPath = *defaultConfigPathResult;

    // Ensure configuration directory exists
    std::error_code ec;
    std::filesystem::create_directories(configPath.parent_path(), ec);
    if (ec) {
        LogW("Failed to create config directory: {}", ec.message());
    }

    // Create driver detection config manager
    DriverDetectionConfigManager configManager(configPath.string());

    // Load configuration
    if (!configManager.loadConfig()) {
        LogW("Failed to load driver detection config, using defaults");
    }

    return configManager;
}

linglong::utils::error::Result<void>
installDriverPackage(const std::vector<GraphicsDriverInfo> &drivers)
{
    LINGLONG_TRACE("installDriverPackage")

    try {
        for (const auto &driverInfo : drivers) {
            LogD("Processing driver: Identify={}, Version={}, Package={}, Installed={}",
                 driverInfo.identify,
                 driverInfo.version,
                 driverInfo.packageName,
                 driverInfo.isInstalled);

            if (driverInfo.isInstalled) {
                LogD("Driver package is already installed: {}", driverInfo.packageName);
                continue;
            }

            // Execute ll-cli install command to install the package
            auto ret = linglong::utils::Cmd("ll-cli").exec({ "install", driverInfo.packageName });
            if (!ret) {
                return LINGLONG_ERR("Installation command failed: " + ret.error().message());
            }

            LogD("Driver package installation command executed successfully: {}", *ret);
        }

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR("Failed to install driver package: " + std::string(e.what()));
    }

    return LINGLONG_OK;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);

    linglong::common::global::applicationInitialize();
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Console);

    DriverDetectOptions options;
    CLI::App cliApp{ "Linglong Graphics Driver Detection Tool" };

    // Define command line options
    cliApp.add_flag("-f,--force", options.force, _("Force installation even if recently reminded"));
    cliApp.add_flag("-c,--check-only",
                    options.checkOnly,
                    _("Check for drivers only without installing or notifying"));
    cliApp.add_flag("-i,--install-only",
                    options.installOnly,
                    _("Only install drivers without notifications"));

    try {
        cliApp.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return cliApp.exit(e);
    }

    LogD("Starting ll-driver-detect application");

    // Initialize application singleton to ensure only one instance runs
    auto defaultConfigPathResult = getDefaultConfigPath();
    if (!defaultConfigPathResult) {
        LogF("Failed to get default config path: {}", defaultConfigPathResult.error().message());
        return 1;
    }

    std::filesystem::path lockFilePath = *defaultConfigPathResult;
    lockFilePath = lockFilePath.parent_path() / "ll-driver-detect.lock";

    ApplicationSingleton singleton(lockFilePath.string());
    auto lockResult = singleton.tryAcquireLock();
    if (!lockResult) {
        LogF("Failed to acquire singleton lock: {}", lockResult.error().message());
        return 1;
    }

    if (!*lockResult) {
        LogW("Another instance of ll-driver-detect is already running. Exiting.");
        return 0; // Exit gracefully, not an error
    }

    auto configManagerResult = setupConfigManager();
    if (!configManagerResult) {
        LogF("Failed to set up configuration: {}", configManagerResult.error().message());
        return 1;
    }

    auto configManager = *configManagerResult;
    if (!options.force && !configManager.shouldShowNotification()) {
        LogD("User has chosen not to be reminded about driver installation");
        return 0;
    }

    // Create driver detection manager for multi-driver support
    DriverDetectionManager detectionManager;

    // Run driver detection and handling for all available drivers
    auto result = detectionManager.detectAllDrivers();

    if (!result) {
        LogF("Driver detection failed: {}", result.error().message());

        return 1;
    }

    const auto &detectionResult = *result;

    if (!detectionResult.hasAvailableDrivers()) {
        std::cout << "No available graphics drivers detected or already installed" << std::endl;
        return 0;
    }

    if (options.checkOnly) {
        std::cout << "CHECK ONLY: only check drivers, no notifications or installations "
                     "will be performed."
                  << std::endl;
        std::cout << "Detected drivers:" << std::endl;
        for (const auto &driverInfo : detectionResult.detectedDrivers) {
            std::cout << "----------------------------------------" << std::endl;
            std::cout << "  Identify: " << driverInfo.identify << std::endl;
            std::cout << "  Version: " << driverInfo.version << std::endl;
            std::cout << "  Package: " << driverInfo.packageName << std::endl;
            std::cout << "  Installed: " << (driverInfo.isInstalled ? "Yes" : "No") << std::endl;
            std::cout << "----------------------------------------" << std::endl;
        }
        return 0;
    }

    LogD("Detected {} graphics driver(s)", detectionResult.detectedDrivers.size());

    if (options.installOnly) {
        std::cout << "Install-only: installing detected drivers without notifications" << std::endl;
        auto installResult = installDriverPackage(detectionResult.detectedDrivers);
        if (!installResult) {
            LogW("Failed to install driver package {}: {}",
                 options.packageName,
                 installResult.error().message());
        }

        std::cout << "Successfully installed driver package" << std::endl;

        return 0;
    }

    // Handle user notifications and installation for each driver
    // Check if we should show notification for this driver
    auto notifier = std::make_unique<DBusNotifier>();
    auto initResult = notifier->init();
    if (!initResult) {
        LogF("Failed to initialize DBus notifier: {}", initResult.error().message());
        return 1;
    }

    // Create driver-specific notification
    DBusNotifier::NotificationRequest request;
    request.appName = "linyaps";
    request.summary = _("Graphics Driver Available");
    request.body = QString(_("Graphics driver package is available that can improve performance "
                             "for some Linyaps applications.\n"
                             "Would you like to install it?"));
    request.actions = QStringList()
      << kActionInstallNow << _("Install Now") << kActionNotRemind << _("Don't Remind");
    request.icon = kNotificationIcon;
    request.timeout = 25000; // 25 seconds

    auto responseResult = notifier->sendInteractiveNotification(request);
    if (!responseResult) {
        LogW("Failed to send notification for driver: {}", responseResult.error().message());
        return 1;
    }

    // Handle user response
    const auto &response = *responseResult;
    if (response.action == kActionInstallNow && response.success) {
        LogD("User chose to install graphics driver");

        auto installResult = installDriverPackage(detectionResult.detectedDrivers);
        if (!installResult) {
            LogW("Failed to install driver package: {}", installResult.error().message());
            return 1;
        }

        LogD("Successfully installed driver package");

        // Send success notification
        DBusNotifier::NotificationRequest successRequest;
        successRequest.appName = "linyaps";
        successRequest.summary = _("Graphics Driver Installation Completed");
        successRequest.body =
          QString(_("Graphics driver package has been installed.\n"
                    "Restart the Linyaps app to experience the performance improvement."));
        successRequest.timeout = 5000; // 5 seconds
        auto successResult = notifier->sendSimpleNotification(successRequest);
        if (!successResult) {
            LogW("Failed to send success notification: {}", successResult.error().message());
        }
    } else if (response.action == kActionNotRemind && response.success) {
        LogD("User chose not to be reminded again");
        configManager.recordUserChoice(UserNotificationChoice::NeverRemind);
        configManager.saveConfig();
    } else {
        LogD("User dismissed notification or timeout, no action taken");
    }

    LogD("Driver detection completed successfully");
    return 0;
}
