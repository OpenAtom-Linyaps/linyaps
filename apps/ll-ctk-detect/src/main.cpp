// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "application_singleton.h"
#include "configure.h"
#include "container_tool_config.h"
#include "container_tool_detector.h"
#include "dbus_notifier.h"
#include "deb_installer.h"
#include "device_detector_registry.h"
#include "linglong/common/dir.h"
#include "linglong/common/global/initialize.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/log/log.h"
#include "nvidia_device_detector.h"

#include <CLI/CLI.hpp>

#include <QCoreApplication>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace linglong::ctk::detect;

constexpr auto kSystemConfigPath = LINGLONG_SYSCONFDIR "/ll-ctk-detect.json";

constexpr auto kActionInstallNow = "install_now";
constexpr auto kActionNotRemind = "not_remind";
constexpr auto kNotificationIcon = "/usr/share/icons/hicolor/scalable/apps/linyaps.svg";

struct CtkDetectOptions
{
    bool checkOnly = false;
};

std::string missingPackageNames(const std::vector<MissingTool> &tools)
{
    std::string names;
    for (const auto &tool : tools) {
        for (const auto &package : tool.packages) {
            if (!names.empty()) {
                names.push_back('\n');
            }
            names += package;
        }
    }
    return names;
}

linglong::utils::error::Result<void> sendSimpleNotification(DBusNotifier &notifier,
                                                            const std::string &summary,
                                                            const std::string &body)
{
    DBusNotifier::NotificationRequest request;
    request.appName = "linyaps";
    request.icon = kNotificationIcon;
    request.summary = summary;
    request.body = body;
    request.timeout = 5000;
    return notifier.sendSimpleNotification(request);
}

} // namespace

int main(int argc, char *argv[])
{
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);

    QCoreApplication app(argc, argv);

    linglong::common::global::applicationInitialize();
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Console);

    CtkDetectOptions options;
    CLI::App cliApp{ "Linglong Container Toolkit Detect Tool" };
    cliApp.add_flag("-c,--check-only",
                    options.checkOnly,
                    _("Check for missing container tools and exit"));

    try {
        cliApp.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return cliApp.exit(e);
    }

    LogD("starting ll-ctk-detect");

    auto userConfigDir = linglong::common::dir::getUserRuntimeConfigDir();
    if (userConfigDir.empty()) {
        LogF("failed to determine user runtime config directory: "
             "neither XDG_CONFIG_HOME nor HOME is set");
        return 1;
    }
    const auto userConfigPath = userConfigDir / "ll-ctk-detect.json";
    const auto lockPath = userConfigDir / "ll-ctk-detect.lock";

    std::unique_ptr<ApplicationSingleton> singleton;
    if (!options.checkOnly) {
        singleton = std::make_unique<ApplicationSingleton>(lockPath.string());
        auto lockResult = singleton->tryAcquireLock();
        if (!lockResult) {
            LogF("failed to acquire singleton lock: {}", lockResult.error().message());
            return 1;
        }
        if (!*lockResult) {
            LogW("another ll-ctk-detect instance is already running; exiting");
            return 0;
        }
    }

    ContainerToolConfigManager configManager(kSystemConfigPath, userConfigPath);
    auto configResult = configManager.load();
    if (!configResult) {
        LogF("failed to load ll-ctk-detect config: {}", configResult.error().message());
        return 1;
    }

    DeviceDetectorRegistry deviceDetectors;
    deviceDetectors.add(std::make_unique<NVIDIADeviceDetector>("nvidia"));
    const auto detectedDeviceIds = deviceDetectors.detectDevices();

    auto missingTools = findMissingContainerTools(configManager.tools(), detectedDeviceIds);

    if (options.checkOnly) {
        if (configManager.tools().empty()) {
            std::cout << _("No container tools configured.") << '\n';
            return 0;
        }

        for (const auto &tool : missingTools) {
            for (const auto &package : tool.packages) {
                std::cout << package << '\n';
            }
        }
        if (missingTools.empty()) {
            std::cout << _("No container tools need to be installed.") << '\n';
        }
        return 0;
    }

    std::vector<MissingTool> pendingTools;
    for (const auto &tool : missingTools) {
        if (!configManager.isNeverRemind(tool.deviceId)) {
            pendingTools.emplace_back(tool);
        }
    }

    if (pendingTools.empty()) {
        LogD("no missing container tools require user action");
        return 0;
    }

    DBusNotifier notifier;
    auto notifierResult = notifier.init();
    if (!notifierResult) {
        LogF("failed to initialize dbus notifier: {}", notifierResult.error().message());
        return 1;
    }

    DBusNotifier::NotificationRequest request;
    request.appName = "linyaps";
    request.summary = _("Container Tool Required");
    request.body = std::string(_("The following container tools need to be installed:\n"))
      + missingPackageNames(pendingTools);
    request.actions = { kActionInstallNow, _("Install Now"), kActionNotRemind, _("Don't Remind") };
    request.icon = kNotificationIcon;
    request.timeout = 25000;

    auto responseResult = notifier.sendInteractiveNotification(request);
    if (!responseResult) {
        LogW("failed to send interactive notification: {}", responseResult.error().message());
        return 1;
    }

    const auto &response = *responseResult;
    if (response.action == kActionNotRemind && response.userInteracted) {
        for (const auto &tool : pendingTools) {
            configManager.setNeverRemind(tool.deviceId, true);
        }
        auto saveResult = configManager.saveUserConfig();
        if (!saveResult) {
            LogW("failed to save user config: {}", saveResult.error().message());
        }
        return 0;
    }

    if (response.action != kActionInstallNow || !response.userInteracted) {
        LogD("user dismissed or timed out; no action taken");
        return 0;
    }

    auto downloadDirResult = createTempDownloadDir();
    if (!downloadDirResult) {
        LogF("failed to create download directory: {}", downloadDirResult.error().message());
        return 1;
    }
    const auto downloadDir = std::move(*downloadDirResult);
    auto cleanupDownloadDir = linglong::utils::finally::finally([&downloadDir] {
        cleanupTempDir(downloadDir);
    });

    struct DownloadedDeb
    {
        std::string package;
        std::filesystem::path path;
    };

    std::vector<DownloadedDeb> downloadedDebs;

    for (const auto &tool : pendingTools) {
        for (const auto &package : tool.packages) {
            auto debResult = downloadDeb(package, downloadDir);
            if (!debResult) {
                LogE("failed to download deb for {}: {}", package, debResult.error().message());
                auto failResult =
                  sendSimpleNotification(notifier,
                                         _("Container Tool Download Failed"),
                                         std::string(_("Failed to download ")) + package + ": "
                                           + debResult.error().message());
                if (!failResult) {
                    LogW("failed to send download failure notification: {}",
                         failResult.error().message());
                }
                return 1;
            }
            downloadedDebs.emplace_back(DownloadedDeb{ package, *debResult });
        }
    }

    for (const auto &deb : downloadedDebs) {
        auto installResult = installDeb(deb.path);
        if (!installResult) {
            LogW("failed to install {}: {}", deb.package, installResult.error().message());
            auto failResult = sendSimpleNotification(
              notifier,
              _("Container Tool Install Failed"),
              std::string(_("Failed to install ")) + deb.package + ".\nPlease try again later.");
            if (!failResult) {
                LogW("failed to send install failure notification: {}",
                     failResult.error().message());
            }
            return 1;
        }
        LogD("installed container tool package {}", deb.package);
    }

    auto success = sendSimpleNotification(
      notifier,
      _("Container Tool Installed"),
      _("Installation completed. Restart the Linyaps app to use the container tool."));
    if (!success) {
        LogW("failed to send install success notification: {}", success.error().message());
    }

    return 0;
}
