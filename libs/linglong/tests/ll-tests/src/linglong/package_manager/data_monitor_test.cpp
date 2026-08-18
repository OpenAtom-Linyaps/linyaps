/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package_manager/data_monitor.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace linglong::service;

TEST(DataMonitor, StartStopAndMeasureSpeed)
{
    std::atomic<int> callbackCount{ 0 };
    DataMonitor monitor(3, 1, [&callbackCount](DataMonitor &) {
        callbackCount++;
    });

    EXPECT_EQ(monitor.getCurrentSpeed(), 0.0);

    monitor.start();
    // starting twice is a no-op
    monitor.start();

    for (int i = 0; i < 6; ++i) {
        monitor.dataArrived(2048);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    monitor.stop();

    EXPECT_GT(monitor.getCurrentSpeed(), 0.0);
    EXPECT_GT(callbackCount.load(), 0);
}

TEST(DataMonitor, StopWithoutStartIsSafe)
{
    DataMonitor monitor(2, 1, [](DataMonitor &) {});
    monitor.stop();
    // repeated stop is safe as well
    DataMonitor monitor2(2, 1, [](DataMonitor &) {});
    monitor2.start();
    monitor2.stop();
    monitor2.stop();
}

TEST(DataMonitor, HumanSpeedFormatting)
{
    DataMonitor monitor(3, 1, [](DataMonitor &) {});

    // No data -> 0 B/s
    EXPECT_EQ(monitor.getHumanSpeed(), "0B/s");

    // Speed is only computed by the monitor loop; keep feeding data so the
    // per-window average stays non-zero.
    monitor.start();
    for (int i = 0; i < 5; ++i) {
        monitor.dataArrived(1024 * 1024);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    EXPECT_NE(monitor.getHumanSpeed(), "0B/s");
    monitor.stop();
}

TEST(DataMonitor, PauseSuppressesCallbacks)
{
    std::atomic<int> callbackCount{ 0 };
    DataMonitor monitor(2, 1, [&callbackCount](DataMonitor &) {
        callbackCount++;
    });

    monitor.pause(true);
    monitor.dataArrived(100);
    monitor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    monitor.stop();

    // callback suppressed while paused
    EXPECT_EQ(callbackCount.load(), 0);
}
