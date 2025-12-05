// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "dbus_notifier.h"

#include <QCoreApplication>

using namespace linglong::driver::detect;

class DBusNotifierTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // QCoreApplication is required for Qt D-Bus to work
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
