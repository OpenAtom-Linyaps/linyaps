// SPDX-FileCopyrightText: 2025-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "dbus_notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariantMap>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <signal.h>
#include <sys/wait.h>

using namespace linglong::driver::detect;

namespace {

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

class FakeNotifications : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    uint nextId{ 42 };
    bool failNotify{ false };
    bool emitAction{ false };
    bool emitClose{ false };
    QString actionKey{ "yes" };
    QString lastSummary;
    QString lastBody;
    QStringList lastActions;
    QVariantMap lastHints;

public Q_SLOTS:

    uint Notify(const QString &appName,
                uint replaceId,
                const QString &icon,
                const QString &summary,
                const QString &body,
                const QStringList &actions,
                const QVariantMap &hints,
                int timeout)
    {
        Q_UNUSED(appName);
        Q_UNUSED(replaceId);
        Q_UNUSED(icon);
        Q_UNUSED(timeout);

        if (failNotify) {
            sendErrorReply(QDBusError::AccessDenied, "notifications are disabled");
            return 0;
        }

        lastSummary = summary;
        lastBody = body;
        lastActions = actions;
        lastHints = hints;

        if (emitAction || emitClose) {
            const uint id = nextId;
            const bool action = emitAction;
            const bool close = emitClose;
            const QString key = actionKey;
            // Queue the emission so it is delivered after Notify returns and the
            // notifier has stored the notification id.
            QTimer::singleShot(0, this, [this, id, action, close, key]() {
                if (action) {
                    Q_EMIT ActionInvoked(id, key);
                }
                if (close) {
                    Q_EMIT NotificationClosed(id, 2);
                }
            });
        }

        return nextId;
    }

    void CloseNotification(uint id) { Q_UNUSED(id); }

    QStringList GetCapabilities() { return { "actions", "body" }; }

Q_SIGNALS:
    void ActionInvoked(uint id, const QString &actionKey);
    void NotificationClosed(uint id, uint reason);
};

} // namespace

class DBusNotifierTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (app != nullptr) {
            return;
        }

        static PrivateBus bus;
        static bool ready = bus.start();
        ASSERT_TRUE(ready) << "failed to start private dbus-daemon";

        char programName[] = "dbus-notifier-test";
        int argc = 1;
        char *argv[] = { programName, nullptr };
        app = new QCoreApplication(argc, argv);

        auto connection = QDBusConnection::connectToBus(QString::fromStdString(bus.busAddress()),
                                                        "qt_default_session_bus");
        ASSERT_TRUE(connection.isConnected())
          << "failed to connect to private session bus at " << bus.busAddress() << ": "
          << connection.lastError().message().toStdString();
        injectedConnectionName = connection.name();
    }

    static void TearDownTestSuite() { QCoreApplication::processEvents(); }

    void SetUp() override
    {
        connection = QDBusConnection::sessionBus();
        ASSERT_TRUE(connection.isConnected());
        EXPECT_EQ(connection.name(), injectedConnectionName);
        fake = std::make_unique<FakeNotifications>();
        ASSERT_TRUE(connection.registerObject("/org/freedesktop/Notifications",
                                              fake.get(),
                                              QDBusConnection::ExportAllSlots
                                                | QDBusConnection::ExportAllSignals))
          << connection.lastError().message().toStdString();
        ASSERT_TRUE(connection.registerService("org.freedesktop.Notifications"))
          << connection.lastError().message().toStdString();
    }

    void TearDown() override
    {
        connection.unregisterObject("/org/freedesktop/Notifications");
        connection.unregisterService("org.freedesktop.Notifications");
        fake.reset();
    }

    static QCoreApplication *app;
    static QString injectedConnectionName;
    QDBusConnection connection{ QString() };
    std::unique_ptr<FakeNotifications> fake;
};

QCoreApplication *DBusNotifierTest::app = nullptr;
QString DBusNotifierTest::injectedConnectionName;

TEST_F(DBusNotifierTest, InitDoesNotCrash)
{
    DBusNotifier notifier;

    auto result = notifier.init();

    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(DBusNotifierTest, SendSimpleNotificationSucceeds)
{
    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.body = "A new driver is available";
    request.appName = "ll-driver-detect";
    request.icon = "nvidia";
    request.timeout = 100;

    auto result = notifier.sendSimpleNotification(request);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(fake->lastSummary, QStringLiteral("NVIDIA driver"));
    EXPECT_EQ(fake->lastBody, QStringLiteral("A new driver is available"));
    EXPECT_TRUE(fake->lastActions.isEmpty());
}

TEST_F(DBusNotifierTest, SendSimpleNotificationReportsServiceFailure)
{
    fake->failNotify = true;

    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.timeout = 100;

    auto result = notifier.sendSimpleNotification(request);

    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("Failed to send notification"), std::string::npos);
}

TEST_F(DBusNotifierTest, InteractiveNotificationReturnsSelectedAction)
{
    fake->emitAction = true;
    fake->emitClose = true;
    fake->actionKey = "install";

    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.actions = QStringList({ "install", "Install", "ignore", "Ignore" });
    request.timeout = 2000;

    auto result = notifier.sendInteractiveNotification(request);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->action, QStringLiteral("install"));
    EXPECT_EQ(fake->lastActions, QStringList({ "install", "Install", "ignore", "Ignore" }));
}

TEST_F(DBusNotifierTest, InteractiveNotificationTimesOutWithoutInteraction)
{
    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.actions = QStringList({ "install", "Install" });
    request.timeout = 200;

    QElapsedTimer timer;
    timer.start();
    auto result = notifier.sendInteractiveNotification(request);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->success);
    EXPECT_TRUE(result->action.isEmpty());
    // QTimer uses a coarse timer by default and may fire up to 5% early.
    EXPECT_GE(timer.elapsed(), static_cast<qint64>(request.timeout) * 9 / 10);
}

TEST_F(DBusNotifierTest, InteractiveNotificationReportsServiceFailure)
{
    fake->failNotify = true;

    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.actions = QStringList({ "install", "Install" });
    request.timeout = 100;

    auto result = notifier.sendInteractiveNotification(request);

    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("Failed to send notification"), std::string::npos);
}

TEST_F(DBusNotifierTest, InteractiveNotificationClosesWithoutAction)
{
    fake->emitClose = true;

    DBusNotifier notifier;
    auto initResult = notifier.init();
    ASSERT_TRUE(initResult.has_value()) << initResult.error().message();

    DBusNotifier::NotificationRequest request;
    request.summary = "NVIDIA driver";
    request.actions = QStringList({ "install", "Install" });
    request.timeout = 2000;

    auto result = notifier.sendInteractiveNotification(request);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->success);
    EXPECT_TRUE(result->action.isEmpty());
}

#include "dbus_notifier_test.moc"
