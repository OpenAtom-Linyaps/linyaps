/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package_manager/package_manager.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QTimer>

namespace {

using linglong::service::PackageManager;

class PackageManagerDaemonModeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        hadDeferredTimeout = qEnvironmentVariableIsSet("LINGLONG_DEFERRED_TIMEOUT");
        deferredTimeout = qgetenv("LINGLONG_DEFERRED_TIMEOUT");
    }

    void TearDown() override
    {
        if (hadDeferredTimeout) {
            qputenv("LINGLONG_DEFERRED_TIMEOUT", deferredTimeout);
        } else {
            qunsetenv("LINGLONG_DEFERRED_TIMEOUT");
        }
    }

    static int timerIntervalFor(const QByteArray &value)
    {
        int argc = 0;
        QCoreApplication app(argc, nullptr);
        qputenv("LINGLONG_DEFERRED_TIMEOUT", value);
        PackageManager manager{ nullptr, nullptr, nullptr };
        manager.initDaemonMode();
        auto *timer = manager.findChild<QTimer *>();
        EXPECT_NE(timer, nullptr);
        return timer == nullptr ? -1 : timer->interval();
    }

    bool hadDeferredTimeout{ false };
    QByteArray deferredTimeout;
};

TEST_F(PackageManagerDaemonModeTest, AcceptsPositiveWholeSeconds)
{
    EXPECT_EQ(timerIntervalFor("17"), 17'000);
}

TEST_F(PackageManagerDaemonModeTest, RejectsNonPositiveValues)
{
    EXPECT_EQ(timerIntervalFor("0"), 3'600'000);
    EXPECT_EQ(timerIntervalFor("-1"), 3'600'000);
}

TEST_F(PackageManagerDaemonModeTest, RejectsTrailingCharactersAndOverflow)
{
    EXPECT_EQ(timerIntervalFor("17seconds"), 3'600'000);
    EXPECT_EQ(timerIntervalFor("2147484"), 3'600'000);
}

} // namespace
