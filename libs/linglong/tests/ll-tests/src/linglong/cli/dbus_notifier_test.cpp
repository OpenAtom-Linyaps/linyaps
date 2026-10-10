/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/cli/dbus_notifier.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>
#include <string>
#include <vector>

namespace linglong::cli::test {

using namespace linglong::cli;

// A minimal org.freedesktop.Notifications stand-in: it answers Notify and then closes
// the notification with a caller-chosen reason, like a real notification server does.
class FakeNotificationService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    FakeNotificationService(quint32 closeReason, std::string invokedAction)
        : m_closeReason(closeReason)
        , m_invokedAction(std::move(invokedAction))
    {
    }

public Q_SLOTS:

    quint32 Notify(const QString &appName,
                   quint32 replaceID,
                   const QString &icon,
                   const QString &summary,
                   const QString &body,
                   const QStringList &actions,
                   const QVariantMap &hints,
                   qint32 expireTimeout)
    {
        Q_UNUSED(appName)
        Q_UNUSED(replaceID)
        Q_UNUSED(icon)
        Q_UNUSED(summary)
        Q_UNUSED(body)
        Q_UNUSED(actions)
        Q_UNUSED(hints)
        Q_UNUSED(expireTimeout)

        const auto id = ++m_lastID;
        // Emit asynchronously so the Notify call returns before the client sees the
        // ActionInvoked/NotificationClosed signals.
        QMetaObject::invokeMethod(
          this,
          [this, id]() {
              if (!m_invokedAction.empty()) {
                  auto invoked = QDBusMessage::createSignal("/org/freedesktop/Notifications",
                                                            "org.freedesktop.Notifications",
                                                            "ActionInvoked");
                  invoked << id << QString::fromStdString(m_invokedAction);
                  QDBusConnection::sessionBus().send(invoked);
              }

              auto closed = QDBusMessage::createSignal("/org/freedesktop/Notifications",
                                                       "org.freedesktop.Notifications",
                                                       "NotificationClosed");
              closed << id << m_closeReason;
              QDBusConnection::sessionBus().send(closed);
          },
          Qt::QueuedConnection);

        return id;
    }

private:
    quint32 m_closeReason;
    std::string m_invokedAction;
    quint32 m_lastID{ 0 };
};

namespace {

linglong::api::types::v1::InteractionRequest makeRequest()
{
    linglong::api::types::v1::InteractionRequest request;
    request.appName = "ll-cli";
    request.summary = "install org.example";
    request.body = "do you want to install org.example 1.0.0?";
    request.actions = std::vector<std::string>{ "install(_I)", "yes", "cancel(_C)", "no" };
    return request;
}

} // namespace

class DBusNotifierRequestTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // QEventLoop and Qt D-Bus need a QCoreApplication instance.
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char arg0[] = "ll-tests";
            static char *argv[] = { arg0, nullptr };
            application = new QCoreApplication(argc, argv);
        }

        if (!connection.isConnected()) {
            GTEST_SKIP() << "no D-Bus session bus available";
        }
    }

    void TearDown() override
    {
        if (connection.isConnected()) {
            connection.unregisterObject("/org/freedesktop/Notifications");
            connection.unregisterService("org.freedesktop.Notifications");
        }
    }

    // Owns org.freedesktop.Notifications with a fake service closing notifications with
    // the given reason. Skips the test when the name cannot be taken over.
    void takeOverBus(quint32 closeReason, const std::string &invokedAction = {})
    {
        service = std::make_unique<FakeNotificationService>(closeReason, invokedAction);
        if (!connection.registerService("org.freedesktop.Notifications")) {
            GTEST_SKIP() << "org.freedesktop.Notifications is already owned on the session bus";
        }
        if (!connection.registerObject("/org/freedesktop/Notifications",
                                       service.get(),
                                       QDBusConnection::ExportAllSlots)) {
            GTEST_SKIP() << "could not register the fake notification service";
        }
    }

    QCoreApplication *application{ nullptr };
    QDBusConnection connection{ QDBusConnection::sessionBus() };
    std::unique_ptr<FakeNotificationService> service;
};

TEST_F(DBusNotifierRequestTest, RequestSurvivesExpiredNotification)
{
    takeOverBus(1 /* Expired */);

    DBusNotifier notifier;

    auto reply = notifier.request(makeRequest());

    // The notification expired before the user chose an action; the notifier must
    // answer with a reply instead of aborting the process.
    ASSERT_TRUE(reply.has_value()) << reply.error().message();
    EXPECT_EQ(reply->action.value_or(""), "");
}

TEST_F(DBusNotifierRequestTest, RequestReturnsInvokedActionWhenServerReportsExpired)
{
    // Servers report Expired when closing a notification after an action was invoked;
    // the collected choice must still be returned.
    takeOverBus(1 /* Expired */, "yes");

    DBusNotifier notifier;

    auto reply = notifier.request(makeRequest());

    ASSERT_TRUE(reply.has_value()) << reply.error().message();
    EXPECT_EQ(reply->action.value_or(""), "yes");
}

TEST_F(DBusNotifierRequestTest, RequestReturnsChoiceWhenDismissed)
{
    takeOverBus(2 /* Dismissed */, "no");

    DBusNotifier notifier;

    auto reply = notifier.request(makeRequest());

    ASSERT_TRUE(reply.has_value()) << reply.error().message();
    EXPECT_EQ(reply->action.value_or(""), "no");
}

} // namespace linglong::cli::test

#include "dbus_notifier_test.moc"
