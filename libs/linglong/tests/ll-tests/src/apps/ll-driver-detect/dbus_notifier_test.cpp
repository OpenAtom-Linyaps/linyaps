// SPDX-FileCopyrightText: 2025-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "dbus_notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <utility>

using namespace linglong::driver::detect;

namespace {

// What the fake daemon should emit after replying to Notify().
struct DaemonBehavior
{
    bool emitAction{ false };          // emit ActionInvoked after Notify
    quint32 actionIdOffset{ 0 };       // ActionInvoked id = notification id + offset
    QString actionKey{ "install" };    // action key carried by ActionInvoked
    bool emitClosed{ false };          // emit NotificationClosed after Notify
    int signalDelayMs{ 200 };          // delay before emitting the signals
};

// Minimal org.freedesktop.Notifications service used to exercise DBusNotifier
// over a real D-Bus session bus. It lives in its own thread with its own bus
// connection because DBusNotifier performs a blocking Notify call on the main
// thread; serving that call from the same thread would deadlock.
class FakeNotificationsService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit FakeNotificationsService(DaemonBehavior behavior, QObject *parent = nullptr)
        : QObject(parent)
        , behavior_(std::move(behavior))
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 Notify(const QString & /*app_name*/,
                                quint32 /*replaces_id*/,
                                const QString & /*app_icon*/,
                                const QString & /*summary*/,
                                const QString & /*body*/,
                                const QStringList & /*actions*/,
                                const QVariantMap & /*hints*/,
                                qint32 /*expire_timeout*/)
    {
        const auto id = ++idCounter;
        QTimer::singleShot(behavior_.signalDelayMs, this, [this, id] {
            if (behavior_.emitAction) {
                Q_EMIT ActionInvoked(id + behavior_.actionIdOffset, behavior_.actionKey);
            }
            if (behavior_.emitClosed) {
                Q_EMIT NotificationClosed(id, 2); // 2 = dismissed by the user
            }
        });

        return id;
    }

Q_SIGNALS:
    Q_SCRIPTABLE void ActionInvoked(quint32 id, QString action_key);
    Q_SCRIPTABLE void NotificationClosed(quint32 id, quint32 reason);

private:
    DaemonBehavior behavior_;
    quint32 idCounter{ 0 };
};

class FakeNotificationsDaemon : public QThread
{
public:
    explicit FakeNotificationsDaemon(DaemonBehavior behavior)
        : behavior_(std::move(behavior))
    {
    }

    ~FakeNotificationsDaemon() override
    {
        quit();
        wait(5000);
    }

protected:
    void run() override
    {
        auto connection =
          QDBusConnection::connectToBus(QDBusConnection::SessionBus, "fake-notifications-daemon");
        if (!connection.isConnected()) {
            return;
        }

        service_ = new FakeNotificationsService(behavior_);
        if (!connection.registerObject("/org/freedesktop/Notifications",
                                       service_,
                                       QDBusConnection::ExportAllContents)
            || !connection.registerService("org.freedesktop.Notifications")) {
            delete service_;
            service_ = nullptr;
            return;
        }

        registered_.store(true);
        exec();

        connection.unregisterService("org.freedesktop.Notifications");
        QDBusConnection::disconnectFromBus("fake-notifications-daemon");
        delete service_;
    }

public:
    bool waitUntilRegistered(int timeoutMs = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (registered_.load()) {
                return true;
            }
            QThread::msleep(20);
        }
        return false;
    }

private:
    DaemonBehavior behavior_;
    FakeNotificationsService *service_{ nullptr };
    std::atomic<bool> registered_{ false };
};

DBusNotifier::NotificationRequest makeRequest(quint32 timeoutMs)
{
    DBusNotifier::NotificationRequest request;
    request.appName = "ll-tests";
    request.summary = "Install driver";
    request.body = "A driver package is available";
    request.actions = QStringList{ "install", "Install" };
    request.timeout = timeoutMs;
    return request;
}

} // namespace

class DBusNotifierTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // CI runners usually have no session bus. Start a private one so the
        // tests exercise real D-Bus traffic instead of being skipped.
        if (qgetenv("DBUS_SESSION_BUS_ADDRESS").isEmpty()) {
            privateBus = new QProcess;
            privateBus->start(QStringLiteral("dbus-daemon"),
                              { QStringLiteral("--session"),
                                QStringLiteral("--print-address=1"),
                                QStringLiteral("--nofork") });
            if (privateBus->waitForStarted(5000)) {
                QByteArray output;
                while (!output.contains('\n') && privateBus->waitForReadyRead(5000)) {
                    output += privateBus->readAll();
                }
                const auto address = output.split('\n').first().trimmed();
                if (!address.isEmpty()) {
                    qputenv("DBUS_SESSION_BUS_ADDRESS", address);
                }
            }
        }

        // QCoreApplication is required for Qt D-Bus to work
        int argc = 1;
        const char *argv[] = { "test" };
        app = new QCoreApplication(argc, const_cast<char **>(argv));
    }

    static void TearDownTestSuite()
    {
        delete app;
        app = nullptr;

        if (privateBus != nullptr) {
            privateBus->terminate();
            privateBus->waitForFinished(5000);
            delete privateBus;
            privateBus = nullptr;
        }
    }

    void SetUp() override
    {
        if (!QDBusConnection::sessionBus().isConnected()) {
            GTEST_SKIP() << "no D-Bus session bus available";
        }

        // The fake daemon cannot claim the name if a real notification service
        // already owns it; skip instead of testing the wrong service.
        auto *busInterface = QDBusConnection::sessionBus().interface();
        if (busInterface != nullptr
            && busInterface->isServiceRegistered(QStringLiteral("org.freedesktop.Notifications"))) {
            GTEST_SKIP() << "a real org.freedesktop.Notifications service is already running";
        }
    }

    static QCoreApplication *app;
    static QProcess *privateBus;
};

QCoreApplication *DBusNotifierTest::app = nullptr;
QProcess *DBusNotifierTest::privateBus = nullptr;

TEST_F(DBusNotifierTest, InitDoesNotCrash)
{
    DBusNotifier notifier;

    // We call init() and primarily test that it does not crash and returns a
    // valid Result object. On systems with a running D-Bus session bus,
    // this is expected to succeed. On systems without one, it is expected
    // to fail gracefully by returning an error.
    auto result = notifier.init();

    if (!result) {
        // If it fails, log the error but don't fail the test.
        // This is an expected outcome if no session bus is available.
        SUCCEED() << "DBusNotifier::init() failed gracefully as expected on this system: "
                  << result.error().message();
    } else {
        // If it succeeds, that's also a valid outcome.
        SUCCEED() << "DBusNotifier::init() succeeded as expected on this system.";
    }
}

// Regression test for https://github.com/OpenAtom-Linyaps/linyaps/issues/1842:
// when the daemon emits ActionInvoked for the matching notification ID without
// a following NotificationClosed, sendInteractiveNotification must return
// promptly with the selected action instead of blocking until the timeout.
TEST_F(DBusNotifierTest, MatchingActionEndsWaitWithoutCloseSignal)
{
    DaemonBehavior behavior;
    behavior.emitAction = true;   // ActionInvoked for the real notification ID
    behavior.emitClosed = false;  // daemon never sends NotificationClosed
    FakeNotificationsDaemon daemon(behavior);
    daemon.start();
    ASSERT_TRUE(daemon.waitUntilRegistered()) << "fake notification service did not register";

    DBusNotifier notifier;
    ASSERT_TRUE(notifier.init().has_value()) << "failed to connect notification signals";

    constexpr quint32 timeoutMs = 5000;
    QElapsedTimer timer;
    timer.start();
    auto result = notifier.sendInteractiveNotification(makeRequest(timeoutMs));
    const auto elapsed = timer.elapsed();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->action, QStringLiteral("install"));
    // Must return shortly after the action signal, not after the full timeout.
    EXPECT_LT(elapsed, timeoutMs - 2000)
      << "call blocked until timeout despite a matching ActionInvoked";
}

// An ActionInvoked whose ID does not match the pending notification must not
// end the wait; the call still falls back to the configured timeout.
TEST_F(DBusNotifierTest, NonMatchingActionDoesNotEndWait)
{
    DaemonBehavior behavior;
    behavior.emitAction = true;
    behavior.actionIdOffset = 100; // ActionInvoked carries an unrelated ID
    behavior.emitClosed = false;
    behavior.signalDelayMs = 100;
    FakeNotificationsDaemon daemon(behavior);
    daemon.start();
    ASSERT_TRUE(daemon.waitUntilRegistered()) << "fake notification service did not register";

    DBusNotifier notifier;
    ASSERT_TRUE(notifier.init().has_value()) << "failed to connect notification signals";

    constexpr quint32 timeoutMs = 800;
    QElapsedTimer timer;
    timer.start();
    auto result = notifier.sendInteractiveNotification(makeRequest(timeoutMs));
    const auto elapsed = timer.elapsed();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->success);
    EXPECT_TRUE(result->action.isEmpty());
    EXPECT_GE(elapsed, timeoutMs - 100)
      << "non-matching ActionInvoked ended the wait prematurely";
    EXPECT_LT(elapsed, timeoutMs + 2000) << "timeout was not honored";
}

// The pre-existing close path keeps working: a matching NotificationClosed
// still ends the wait without user interaction.
TEST_F(DBusNotifierTest, NotificationClosedStillEndsWait)
{
    DaemonBehavior behavior;
    behavior.emitAction = false;
    behavior.emitClosed = true;
    behavior.signalDelayMs = 200;
    FakeNotificationsDaemon daemon(behavior);
    daemon.start();
    ASSERT_TRUE(daemon.waitUntilRegistered()) << "fake notification service did not register";

    DBusNotifier notifier;
    ASSERT_TRUE(notifier.init().has_value()) << "failed to connect notification signals";

    constexpr quint32 timeoutMs = 5000;
    QElapsedTimer timer;
    timer.start();
    auto result = notifier.sendInteractiveNotification(makeRequest(timeoutMs));
    const auto elapsed = timer.elapsed();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->success);
    EXPECT_TRUE(result->action.isEmpty());
    EXPECT_LT(elapsed, timeoutMs - 2000)
      << "matching NotificationClosed no longer ends the wait";
}

#include "dbus_notifier_test.moc"
