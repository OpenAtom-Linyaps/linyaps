// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "deb_installer.h"

#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <vector>

#include <unistd.h>

namespace linglong::ctk::detect {

namespace {

utils::error::Result<std::filesystem::path> makePrivateDir(const std::filesystem::path &baseDir,
                                                           const std::string &prefix)
{
    LINGLONG_TRACE("create private download directory")

    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create download base directory: " + ec.message());
    }

    auto templatePath = baseDir / (prefix + "XXXXXX");
    std::string rawTemplate = templatePath.string();
    std::vector<char> buffer(rawTemplate.begin(), rawTemplate.end());
    buffer.push_back('\0');

    if (::mkdtemp(buffer.data()) == nullptr) {
        return LINGLONG_ERR("failed to create private download directory: "
                            + std::string(::strerror(errno)));
    }

    return std::filesystem::path(buffer.data());
}

utils::error::Result<std::filesystem::path> findDownloadedDeb(const std::filesystem::path &dir)
{
    LINGLONG_TRACE("find downloaded deb package")

    std::error_code ec;
    std::optional<std::filesystem::path> debPath;

    for (std::filesystem::directory_iterator iter(dir, ec), end; !ec && iter != end;
         iter.increment(ec)) {
        if (iter->path().extension() != ".deb") {
            continue;
        }
        if (debPath) {
            return LINGLONG_ERR("multiple .deb files found in download directory: " + dir.string());
        }
        debPath = iter->path();
    }

    if (ec) {
        return LINGLONG_ERR("failed to inspect download directory " + dir.string() + ": "
                            + ec.message());
    }

    if (!debPath) {
        return LINGLONG_ERR("no .deb file found after downloading package in " + dir.string());
    }

    return *debPath;
}

} // namespace

utils::error::Result<std::filesystem::path> createTempDownloadDir()
{
    return makePrivateDir(std::filesystem::temp_directory_path(),
                          "ll-ctk-detect-" + std::to_string(::getuid()) + "-");
}

utils::error::Result<std::filesystem::path> downloadDeb(const std::string &package,
                                                        const std::filesystem::path &downloadDir)
{
    LINGLONG_TRACE("download deb package: " + package)

    auto workingDir = makePrivateDir(downloadDir, "package-");
    if (!workingDir) {
        return LINGLONG_ERR(workingDir);
    }

    std::error_code ec;
    const auto previousWorkingDir = std::filesystem::current_path(ec);
    if (ec) {
        return LINGLONG_ERR("failed to read current working directory: " + ec.message());
    }

    std::filesystem::current_path(*workingDir, ec);
    if (ec) {
        return LINGLONG_ERR("failed to enter download directory " + workingDir->string() + ": "
                            + ec.message());
    }

    auto command = linglong::utils::Cmd("timeout");
    auto result = command.exec({ "--kill-after=5s", "5m", "apt-get", "download", package });

    std::filesystem::current_path(previousWorkingDir, ec);
    if (ec) {
        return LINGLONG_ERR("failed to restore current working directory: " + ec.message());
    }

    if (!result) {
        return LINGLONG_ERR("apt-get download failed for package " + package + ": "
                            + result.error().message());
    }

    return findDownloadedDeb(*workingDir);
}

utils::error::Result<void> installDeb(const std::filesystem::path &debPath)
{
    LINGLONG_TRACE("install deb via com.deepin.DebInstaller")

    QDBusInterface dbus("com.deepin.DebInstaller",
                        "/com/deepin/DebInstaller",
                        "com.deepin.DebInstaller",
                        QDBusConnection::sessionBus());
    if (!dbus.isValid()) {
        return LINGLONG_ERR("com.deepin.DebInstaller DBus interface is not valid");
    }

    QDBusMessage reply = dbus.call("InstallerDebPackge", QString::fromStdString(debPath.string()));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return LINGLONG_ERR("com.deepin.DebInstaller failed: "
                            + reply.errorMessage().toStdString());
    }

    if (reply.arguments().isEmpty()) {
        return LINGLONG_ERR("com.deepin.DebInstaller returned an unexpected result for "
                            + debPath.string());
    }

    const auto result = reply.arguments().first().toString();
    if (result != QStringLiteral("install succeeded")) {
        return LINGLONG_ERR("com.deepin.DebInstaller failed to install " + debPath.string() + ": "
                            + result.toStdString());
    }

    return LINGLONG_OK;
}

void cleanupTempDir(const std::filesystem::path &dir)
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (ec) {
        LogW("failed to clean temp dir {}: {}", dir.string(), ec.message());
    }
}

} // namespace linglong::ctk::detect
