/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/dbus_notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using namespace linglong;
namespace v1 = api::types::v1;

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

    ~PrivateBus()
    {
        if (pid > 0) {
            ::kill(pid, SIGTERM);
            int status = 0;
            ::waitpid(pid, &status, 0);
        }
    }

    const std::string &busAddress() const { return address; }

private:
    pid_t pid{ -1 };
    std::string address;
};

PrivateBus &privateBus()
{
    static PrivateBus bus;
    static const bool started = bus.start();
    (void)started;
    return bus;
}

class CliDBusNotifierTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto &bus = privateBus();
        if (bus.busAddress().empty()) {
            GTEST_SKIP() << "a private dbus-daemon is required for dbus notifier tests";
        }

        if (QCoreApplication::instance() == nullptr) {
            static int argc = 0;
            application = std::make_unique<QCoreApplication>(argc, nullptr);
        }

        if (!QDBusConnection::connectToBus(QString::fromStdString(bus.busAddress()),
                                           QStringLiteral("qt_default_session_bus"))
               .isConnected()) {
            GTEST_SKIP() << "failed to connect to the private session bus";
        }
    }

    void TearDown() override
    {
        // Keep the private bus address and the single QCoreApplication alive for
        // the rest of the process; Qt does not support recreating them.
        application.release();
    }

    std::unique_ptr<QCoreApplication> application;
};

TEST_F(CliDBusNotifierTest, NotifyReportsServiceFailure)
{
    cli::DBusNotifier notifier;
    v1::InteractionRequest request{
        .actions = std::vector<std::string>{ "yes", "no" },
        .appName = "ll-cli",
        .body = "install it?",
        .summary = "confirm",
    };

    auto result = notifier.notify(request);
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("failed to notify"));
}

TEST_F(CliDBusNotifierTest, RequestReportsServiceFailureBeforeWaiting)
{
    cli::DBusNotifier notifier;
    v1::InteractionRequest request{
        .actions = std::vector<std::string>{ "yes", "no" },
        .appName = "ll-cli",
        .summary = "confirm",
    };

    auto reply = notifier.request(request);
    ASSERT_FALSE(reply);
    EXPECT_THAT(reply.error().message(), ::testing::HasSubstr("failed to notify"));
}

} // namespace
