/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/serialize/json.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <signal.h>
#include <sys/wait.h>

using namespace linglong::common::serialize;

TEST(JsonObjectfromVariantMap, DecodesScalarValues)
{
    QVariantMap input{
        { "str", QVariant(QStringLiteral("hello")) },
        { "emptyStr", QVariant(QStringLiteral("")) },
        { "int", QVariant(42) },
        { "boolTrue", QVariant(true) },
        { "boolFalse", QVariant(false) },
        { "double", QVariant(3.5) },
        { "stringList", QVariantList{ QStringLiteral("a"), QStringLiteral("b") } },
    };

    const auto decoded = QJsonObjectfromVariantMap(input);

    EXPECT_EQ(decoded.value(QStringLiteral("str")).toString(), QStringLiteral("hello"));
    EXPECT_EQ(decoded.value(QStringLiteral("emptyStr")).toString(), QString());
    EXPECT_EQ(decoded.value(QStringLiteral("int")).toInt(), 42);
    EXPECT_EQ(decoded.value(QStringLiteral("boolTrue")).toBool(), true);
    EXPECT_EQ(decoded.value(QStringLiteral("boolFalse")).toBool(), false);
    EXPECT_DOUBLE_EQ(decoded.value(QStringLiteral("double")).toDouble(), 3.5);

    const auto stringList = decoded.value(QStringLiteral("stringList")).toArray();
    ASSERT_EQ(stringList.size(), 2);
    EXPECT_EQ(stringList.at(0).toString(), QStringLiteral("a"));
    EXPECT_EQ(stringList.at(1).toString(), QStringLiteral("b"));
}

TEST(JsonObjectfromVariantMap, DecodesNestedMap)
{
    QVariantMap input{
        { "nested",
          QVariantMap{
            { "name", QVariant(QStringLiteral("zoom")) },
            { "count", QVariant(3) },
          } },
    };

    const auto decoded = QJsonObjectfromVariantMap(input);
    const auto nested = decoded.value(QStringLiteral("nested")).toObject();

    EXPECT_EQ(nested.value(QStringLiteral("name")).toString(), QStringLiteral("zoom"));
    EXPECT_EQ(nested.value(QStringLiteral("count")).toInt(), 3);
}

TEST(JsonObjectfromVariantMap, DecodesNestedArray)
{
    QVariantMap inner{
        { "name", QVariant(QStringLiteral("inner")) },
    };
    QVariantMap input{
        { "array",
          QVariantList{
            QVariant(1),
            QVariant(QStringLiteral("two")),
            inner,
          } },
    };

    const auto decoded = QJsonObjectfromVariantMap(input);
    const auto array = decoded.value(QStringLiteral("array")).toArray();

    ASSERT_EQ(array.size(), 3);
    EXPECT_EQ(array.at(0).toInt(), 1);
    EXPECT_EQ(array.at(1).toString(), QStringLiteral("two"));
    EXPECT_EQ(array.at(2).toObject().value(QStringLiteral("name")).toString(),
              QStringLiteral("inner"));
}

TEST(JsonObjectfromVariantMap, DecodesMixedTreeToExpectedJson)
{
    QVariantMap config{
        { "channel", QVariant(QStringLiteral("main")) },
        { "verbose", QVariant(true) },
        { "retries", QVariant(3) },
        { "tags", QVariantList{ QStringLiteral("x86_64"), QStringLiteral("arm64") } },
        { "repo",
          QVariantMap{
            { "url", QVariant(QStringLiteral("https://example.com/repo")) },
            { "priority", QVariant(0) },
          } },
    };

    const auto decoded = QJsonObjectfromVariantMap(config);
    const QJsonObject expected{
        { "channel", QJsonValue(QStringLiteral("main")) },
        { "verbose", QJsonValue(true) },
        { "retries", QJsonValue(3) },
        { "tags",
          QJsonArray{ QJsonValue(QStringLiteral("x86_64")), QJsonValue(QStringLiteral("arm64")) } },
        { "repo",
          QJsonObject{
            { "url", QJsonValue(QStringLiteral("https://example.com/repo")) },
            { "priority", QJsonValue(0) },
          } },
    };

    EXPECT_EQ(decoded, expected);
}

TEST(JsonObjectfromVariantMap, DecodesEmptyContainers)
{
    QVariantMap input{
        { "map", QVariantMap{} },
        { "list", QVariantList{} },
    };

    const auto decoded = QJsonObjectfromVariantMap(input);

    EXPECT_TRUE(decoded.value(QStringLiteral("map")).toObject().isEmpty());
    EXPECT_TRUE(decoded.value(QStringLiteral("list")).toArray().isEmpty());
}

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

class PayloadService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.linglong.json.Test")

public:
    explicit PayloadService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public Q_SLOTS:

    // A map whose values come back over D-Bus as QVariant-wrapped
    // QDBusArgument entries for the nested map and the nested array.
    QVariantMap payload() const
    {
        QVariantMap inner{
            { "name", QVariant(QStringLiteral("ng")) },
            { "count", QVariant(3) },
        };
        QVariantMap entry{
            { "key", QVariant(QStringLiteral("deep")) },
        };
        QVariantList array{
            QVariant(1),
            QVariant(QStringLiteral("two")),
            entry,
        };

        return QVariantMap{
            { "scalar", QVariant(QStringLiteral("plain")) },
            { "nested", inner },
            { "array", array },
        };
    }
};

class JsonFromDBusFixture : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (app != nullptr) {
            return;
        }

        auto &bus = privateBus();
        ASSERT_TRUE(bus.start()) << "failed to start a private dbus-daemon";

        char programName[] = "json-serialize-test";
        int argc = 1;
        char *argv[] = { programName, nullptr };
        app = new QCoreApplication(argc, argv);

        connection = new QDBusConnection(
          QDBusConnection::connectToBus(QString::fromStdString(bus.busAddress()),
                                        "qt_default_session_bus"));
        ASSERT_TRUE(connection->isConnected()) << "failed to connect to the private session bus: "
                                               << connection->lastError().message().toStdString();
    }

    static void TearDownTestSuite()
    {
        delete connection;
        connection = nullptr;
        delete app;
        app = nullptr;
        privateBus().stop();
    }

    void SetUp() override
    {
        ASSERT_TRUE(connection->isConnected());

        service = std::make_unique<PayloadService>();
        ASSERT_TRUE(connection->registerObject(QStringLiteral("/org/linglong/json/Test"),
                                               service.get(),
                                               QDBusConnection::ExportAllSlots))
          << connection->lastError().message().toStdString();
        ASSERT_TRUE(connection->registerService(QStringLiteral("org.linglong.json.Test")))
          << connection->lastError().message().toStdString();
    }

    void TearDown() override
    {
        connection->unregisterObject(QStringLiteral("/org/linglong/json/Test"));
        connection->unregisterService(QStringLiteral("org.linglong.json.Test"));
        service.reset();
    }

    QVariantMap fetchPayload() const
    {
        QDBusInterface iface(QStringLiteral("org.linglong.json.Test"),
                             QStringLiteral("/org/linglong/json/Test"),
                             QStringLiteral("org.linglong.json.Test"),
                             *connection);
        QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("payload"));
        return reply.value();
    }

    static QDBusConnection *connection;
    static QCoreApplication *app;

    std::unique_ptr<PayloadService> service;

    static PrivateBus &privateBus()
    {
        static PrivateBus bus;
        return bus;
    }
};

QCoreApplication *JsonFromDBusFixture::app = nullptr;
QDBusConnection *JsonFromDBusFixture::connection = nullptr;

} // namespace

TEST_F(JsonFromDBusFixture, DecodesMapTypeArgument)
{
    const auto reply = fetchPayload();

    const auto decoded = QJsonObjectfromVariantMap(reply);
    const auto nested = decoded.value(QStringLiteral("nested")).toObject();

    EXPECT_EQ(decoded.value(QStringLiteral("scalar")).toString(), QStringLiteral("plain"));
    ASSERT_FALSE(nested.isEmpty());
    EXPECT_EQ(nested.value(QStringLiteral("name")).toString(), QStringLiteral("ng"));
    EXPECT_EQ(nested.value(QStringLiteral("count")).toInt(), 3);
}

TEST_F(JsonFromDBusFixture, DecodesArrayTypeArgument)
{
    const auto reply = fetchPayload();

    const auto decoded = QJsonObjectfromVariantMap(reply);
    const auto array = decoded.value(QStringLiteral("array")).toArray();

    ASSERT_EQ(array.size(), 3);
    EXPECT_EQ(array.at(0).toInt(), 1);
    EXPECT_EQ(array.at(1).toString(), QStringLiteral("two"));
    // The third element is a nested map recovered from a nested
    // QDBusArgument, i.e. the recursive conversion path.
    EXPECT_EQ(array.at(2).toObject().value(QStringLiteral("key")).toString(),
              QStringLiteral("deep"));
}

TEST_F(JsonFromDBusFixture, DecodesReplyToExpectedJson)
{
    const auto reply = fetchPayload();

    const auto decoded = QJsonObjectfromVariantMap(reply);
    const QJsonObject expected{
        { "scalar", QJsonValue(QStringLiteral("plain")) },
        { "nested",
          QJsonObject{
            { "name", QJsonValue(QStringLiteral("ng")) },
            { "count", QJsonValue(3) },
          } },
        { "array",
          QJsonArray{ QJsonValue(1),
                      QJsonValue(QStringLiteral("two")),
                      QJsonObject{ { "key", QJsonValue(QStringLiteral("deep")) } } } },
    };

    EXPECT_EQ(decoded, expected);
}

#include "json_test.moc"
