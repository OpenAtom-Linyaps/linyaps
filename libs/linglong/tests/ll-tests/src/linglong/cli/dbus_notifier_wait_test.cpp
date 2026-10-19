// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/cli/dbus_notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>

#include <memory>

namespace linglong::cli::test {

class WaitNotificationService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    static constexpr quint32 id = 42;
    bool sendAction = false;
    bool sendUnrelatedClose = false;

public Q_SLOTS:

    quint32 Notify(const QString &,
                   quint32,
                   const QString &,
                   const QString &,
                   const QString &,
                   const QStringList &,
                   const QVariantMap &,
                   int)
    {
        if (sendUnrelatedClose) {
            QTimer::singleShot(10, this, [this] {
                Q_EMIT NotificationClosed(id + 1, 2);
            });
        }
        if (sendAction) {
            QTimer::singleShot(30, this, [this] {
                Q_EMIT ActionInvoked(id, "yes");
            });
        }
        QTimer::singleShot(100, this, [this] {
            Q_EMIT NotificationClosed(id, 2);
        });
        return id;
    }

Q_SIGNALS:
    void ActionInvoked(quint32 id, const QString &action);
    void NotificationClosed(quint32 id, quint32 reason);
};

class LoopMetaCallCounter : public QObject
{
public:
    int count = 0;

protected:
    bool eventFilter(QObject *receiver, QEvent *event) override
    {
        if (event->type() == QEvent::MetaCall && qobject_cast<QEventLoop *>(receiver)) {
            ++count;
        }
        return false;
    }
};

class DBusNotifierWaitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QCoreApplication::instance()) {
            application = std::make_unique<QCoreApplication>(argc, argv);
        }
        bus = std::make_unique<QDBusConnection>(QDBusConnection::sessionBus());
        if (!bus->isConnected()) {
            GTEST_SKIP() << "run under dbus-run-session to exercise notification waiting";
        }
        if (!bus->registerService("org.freedesktop.Notifications")) {
            GTEST_SKIP() << "notification service name is already owned";
        }
        ownsService = true;
        ASSERT_TRUE(
          bus->registerObject("/org/freedesktop/Notifications",
                              &service,
                              QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    }

    void TearDown() override
    {
        if (ownsService) {
            bus->unregisterObject("/org/freedesktop/Notifications");
            bus->unregisterService("org.freedesktop.Notifications");
        }
        bus.reset();
        application.reset();
    }

    api::types::v1::InteractionRequest request() const
    {
        api::types::v1::InteractionRequest value;
        value.appName = "linyaps-test";
        value.summary = "test";
        value.actions = std::vector<std::string>{ "yes", "Yes", "no", "No" };
        return value;
    }

    int argc = 1;
    char name[16] = "notifier-test";
    char *argv[2] = { name, nullptr };
    std::unique_ptr<QCoreApplication> application;
    std::unique_ptr<QDBusConnection> bus;
    WaitNotificationService service;
    bool ownsService = false;
};

TEST_F(DBusNotifierWaitTest, WaitDoesNotContinuouslyQueueLoopCallbacks)
{
    DBusNotifier notifier;
    LoopMetaCallCounter counter;
    QCoreApplication::instance()->installEventFilter(&counter);
    auto result = notifier.request(request());
    QCoreApplication::instance()->removeEventFilter(&counter);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_LT(counter.count, 20) << "notification waiting must not busy-poll the Qt event queue";
}

TEST_F(DBusNotifierWaitTest, IgnoresUnrelatedClosureAndRetainsActionUntilMatchingClosure)
{
    service.sendAction = true;
    service.sendUnrelatedClose = true;
    DBusNotifier notifier;
    QElapsedTimer elapsed;
    elapsed.start();
    auto result = notifier.request(request());
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(result->action.has_value());
    EXPECT_EQ(*result->action, "yes");
    EXPECT_GE(elapsed.elapsed(), 70);
}

} // namespace linglong::cli::test

#include "dbus_notifier_wait_test.moc"
