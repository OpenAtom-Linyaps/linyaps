// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "dbus_notifier.h"

#include <QCoreApplication>

using namespace linglong::ctk::detect;

class DBusNotifierTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        int argc = 1;
        const char *argv[] = { "test" };
        app = new QCoreApplication(argc, const_cast<char **>(argv));
    }

    static void TearDownTestSuite()
    {
        delete app;
        app = nullptr;
    }

    static QCoreApplication *app;
};

QCoreApplication *DBusNotifierTest::app = nullptr;

TEST_F(DBusNotifierTest, InitDoesNotCrash)
{
    DBusNotifier notifier;
    auto result = notifier.init();
    if (!result) {
        SUCCEED() << "DBusNotifier::init() failed gracefully as expected on this system: "
                  << result.error().message();
    } else {
        SUCCEED() << "DBusNotifier::init() succeeded as expected on this system.";
    }
}
