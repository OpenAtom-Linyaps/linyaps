// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "cache_dialog.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/gettext.h"
#include "permissionDialog.h"
#include "tl/expected.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QSocketNotifier>
#include <QTimer>
#include <QWidget>

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr std::string_view protocol_version = "1.0";

int writeMessage(const linglong::api::types::v1::DialogMessage &message) noexcept
{
    auto content = nlohmann::json(message).dump();
    uint32_t size = content.size();
    std::string rawData{ reinterpret_cast<char *>(&size), 4 };
    rawData.append(content);

    std::string::size_type offset{ 0 };
    auto expectedWrite = rawData.size();
    while (true) {
        auto bytesWrite = ::write(STDOUT_FILENO, rawData.data() + offset, expectedWrite - offset);
        if (bytesWrite == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "write failed:" << ::strerror(errno) << std::endl;
            return -1;
        }

        offset += bytesWrite;
        if (offset == expectedWrite) {
            break;
        }
    }

    return 0;
}

tl::expected<linglong::api::types::v1::DialogMessage, int> readMessage() noexcept
{
    using namespace std::chrono_literals;
    uint32_t len{ 0 };
    uint32_t curLen{ 0 };
    std::string rawData;
    QEventLoop loop;

    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&loop] {
        std::cerr << "TIMEOUT" << std::endl;
        loop.exit(-1);
    });

    QObject::connect(&notifier,
                     &QSocketNotifier::activated,
                     [&loop, &len, &curLen, &timer, &rawData] {
                         int32_t bytes{ 0 };
                         ssize_t bytesRead{ 0 };

                         if (curLen < 4) {
                             bytes = 4 - curLen; // NOLINT
                             bytesRead = ::read(STDIN_FILENO,
                                                reinterpret_cast<unsigned char *>(&len) + curLen,
                                                bytes);
                             if (bytesRead == -1) {
                                 if (errno == EINTR) {
                                     timer.start(4s);
                                     return;
                                 }

                                 std::cerr << "read failed:" << ::strerror(errno) << std::endl;
                                 loop.exit(errno);
                                 return;
                             }

                             curLen += bytesRead;
                             if (bytesRead != bytes) {
                                 timer.start(4s);
                                 return;
                             }

                             rawData.resize(len);
                         }

                         bytes = len - (curLen - 4); // NOLINT
                         bytesRead = ::read(STDIN_FILENO, rawData.data() + (curLen - 4), bytes);
                         if (bytesRead == -1) {
                             if (errno == EINTR) {
                                 timer.start(4s);
                                 return;
                             }

                             std::cerr << "read failed:" << ::strerror(errno) << std::endl;
                             loop.exit(errno);
                             return;
                         }

                         if (bytesRead != bytes) {
                             timer.start(4s);
                             return;
                         }

                         loop.exit(0);
                     });
    timer.start(4s);

    if (auto ret = loop.exec(); ret != 0) {
        return tl::unexpected{ ret };
    }

    linglong::api::types::v1::DialogMessage message;
    try {
        message = nlohmann::json::parse(rawData).get<linglong::api::types::v1::DialogMessage>();
    } catch (const std::exception &e) {
        std::cerr << "caught exception:" << e.what() << std::endl;
        return tl::unexpected{ -1 };
    }

    return message;
}

int showPermissionDialog()
{
    auto payload = nlohmann::json(linglong::api::types::v1::DialogHandShakePayload{
                                    .version = std::string{ protocol_version } })
                     .dump();
    auto handshake =
      linglong::api::types::v1::DialogMessage{ .payload = std::move(payload), .type = "Handshake" };
    if (writeMessage(handshake) == -1) {
        return -1;
    }

    auto request = readMessage();
    if (!request) {
        std::cerr << "failed to read message" << std::endl;
        return -1;
    }

    if (request->type != "Request") {
        std::cerr << "unexpected message type:" << request->type << std::endl;
        return -1;
    }

    linglong::api::types::v1::ApplicationPermissionsRequest perms;
    try {
        auto required = nlohmann::json::parse(request->payload);
        auto content = required.get<linglong::api::types::v1::ApplicationPermissionsRequest>();
        perms = std::move(content);
    } catch (const std::exception &e) {
        std::cerr << "caught exception:" << e.what() << std::endl;
        return -1;
    }

    // let memory leak, os will recycle it
    auto *dialog = new PermissionDialog{ perms };
    dialog->show();

    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_EnableHighDpiScaling);
#endif
    QApplication app{ argc, argv };
    Q_INIT_RESOURCE(cache_dialog_resource);
    QApplication::setApplicationName("ll-dialog");
    QApplication::setApplicationVersion(LINGLONG_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("dialog for interaction");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption modeOption(QStringList{ "m", "mode" }, "setting dialog mode", "mode");
    QCommandLineOption idOption("id", "Application id", "id");
    parser.addOption(modeOption);
    parser.addOption(idOption);
    parser.process(app);

    auto mode = parser.value(modeOption);
    if (mode.isEmpty()) {
        return -1;
    }

    if (mode == "permission") {
        if (showPermissionDialog() != 0) {
            return -1;
        }
    }

    if (mode == "startup") {
        auto id = parser.value(idOption);
        if (id.isEmpty()) {
            parser.showHelp();
            return -1;
        }
        auto *dialog = new CacheDialog(id);
        dialog->show();
    }

    return QApplication::exec();
}
