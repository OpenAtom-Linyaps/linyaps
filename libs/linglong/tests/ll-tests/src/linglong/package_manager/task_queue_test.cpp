/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/State.hpp"
#include "linglong/package_manager/package_task.h"
#include "linglong/utils/log/log.h"

#include <QCoreApplication>
#include <QObject>

#include <thread>

namespace {

using namespace linglong::service;
using ::testing::_;
using ::testing::MockFunction;

TEST(TaskQueue, addTask)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTaskQueue queue(nullptr);
    auto ret = queue.addTask([](Task &task) {
        task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
    });
    EXPECT_TRUE(ret);

    QObject::connect(&queue, &PackageTaskQueue::taskDone, [&queue, &app](const QString &taskID) {
        // test delay handle taskDone signal
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto task = queue.getTask(taskID.toStdString());
        EXPECT_TRUE(task);
        EXPECT_EQ(task->get().state(), linglong::api::types::v1::State::Succeed);
        EXPECT_EQ(task->get().message(), "succeeded");

        app.quit();
    });

    app.exec();
}

TEST(TaskQueue, joinTask)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    int called = 0;
    {
        PackageTaskQueue queue(nullptr);
        auto ret = queue.addTask([&app](Task &task) {
            // quit application, so next task will not be run
            app.quit();

            std::this_thread::sleep_for(std::chrono::seconds(1));

            task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        });
        EXPECT_TRUE(ret);

        ret = queue.addTask([](Task &task) {
            task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        });
        EXPECT_TRUE(ret);

        QObject::connect(&queue,
                         &PackageTaskQueue::taskDone,
                         [&called]([[maybe_unused]] const QString &taskID) {
                             called++;
                         });
        app.exec();
    }

    EXPECT_EQ(called, 1);
}

} // namespace
