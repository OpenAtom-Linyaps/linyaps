/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/serialize/json.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/utils/log/log.h"

#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

namespace {

using namespace linglong::service;
using ::testing::_;

template <typename Predicate>
bool processEventsUntil(Predicate &&predicate,
                        std::chrono::milliseconds timeout = std::chrono::seconds(1))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }
    return predicate();
}

TEST(PackageTask, emitsTypedEventsAndFinishesOnce)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    std::vector<std::pair<QString, QVariantMap>> events;
    std::vector<QVariantMap> results;
    auto ret = queue.addPackageTask([](Task &task) {
        task.updateProgress(25, "downloading");
        task.updateStateMessage("verifying");
        task.sendMessage("a standalone message");
        task.updateState(linglong::api::types::v1::State::Processing, "processing");
        task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        task.updateState(linglong::api::types::v1::State::Succeed, "still succeeded");
    });
    ASSERT_TRUE(ret);
    auto &task = ret->get();
    QObject::connect(&task,
                     &PackageTask::TaskEvent,
                     [&events](const QString &event, const QVariantMap &data) {
                         events.emplace_back(event, data);
                     });
    QObject::connect(&task, &PackageTask::TaskFinished, [&results](const QVariantMap &result) {
        results.push_back(result);
    });

    ASSERT_TRUE(processEventsUntil([&results]() {
        return !results.empty();
    }));

    ASSERT_EQ(events.size(), 5U);
    EXPECT_EQ(events[0].first, QStringLiteral("state"));
    auto state = linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
      events[0].second);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->state, linglong::api::types::v1::State::Queued);
    EXPECT_DOUBLE_EQ(state->progress, 25);
    EXPECT_EQ(state->message, "downloading");

    EXPECT_EQ(events[1].first, QStringLiteral("state"));
    state = linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
      events[1].second);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->state, linglong::api::types::v1::State::Queued);
    EXPECT_DOUBLE_EQ(state->progress, 25);
    EXPECT_EQ(state->message, "verifying");

    EXPECT_EQ(events[2].first, QStringLiteral("message"));
    EXPECT_EQ(events[2].second.value(QStringLiteral("message")).toString(),
              QStringLiteral("a standalone message"));

    EXPECT_EQ(events[3].first, QStringLiteral("state"));
    state = linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
      events[3].second);
    ASSERT_TRUE(state);
    EXPECT_EQ(state->state, linglong::api::types::v1::State::Processing);
    EXPECT_DOUBLE_EQ(state->progress, 25);
    EXPECT_EQ(state->message, "processing");

    ASSERT_EQ(results.size(), 1U);
    EXPECT_FALSE(results[0].contains(QStringLiteral("state")));
    EXPECT_TRUE(results[0].value(QStringLiteral("type")).toString().isEmpty());
    EXPECT_EQ(results[0].value(QStringLiteral("code")).toInt(),
              static_cast<int>(linglong::utils::error::ErrorCode::Success));
    EXPECT_EQ(results[0].value(QStringLiteral("message")).toString(), QStringLiteral("succeeded"));
}

TEST(PackageTask, startIsIdempotent)
{
    PackageTask task({});
    task.setState(linglong::api::types::v1::State::Pending);
    int starts = 0;
    QObject::connect(&task, &PackageTask::startRequested, [&starts]() {
        starts++;
    });

    task.Start();
    task.Start();

    EXPECT_EQ(task.state(), linglong::api::types::v1::State::Queued);
    EXPECT_EQ(starts, 1);
}

TEST(PackageTask, emitsConcreteResultSelectedByType)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    QVariantMap result;
    auto ret = queue.addPackageTask([](Task &task) {
        auto &packageTask = dynamic_cast<PackageTask &>(task);
        packageTask.setResult({ { QStringLiteral("type"), QStringLiteral("ExampleResult") },
                                { QStringLiteral("value"), 42 } });
        task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
    });
    ASSERT_TRUE(ret);
    auto &task = ret->get();
    QObject::connect(&task, &PackageTask::TaskFinished, [&result](const QVariantMap &value) {
        result = value;
    });

    ASSERT_TRUE(processEventsUntil([&result]() {
        return !result.empty();
    }));

    EXPECT_EQ(result.value(QStringLiteral("type")).toString(), QStringLiteral("ExampleResult"));
    EXPECT_EQ(result.value(QStringLiteral("value")).toInt(), 42);
    EXPECT_FALSE(result.contains(QStringLiteral("state")));
}

TEST(PackageTask, canceledTaskIgnoresConcreteResult)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    QVariantMap result;
    auto ret = queue.addPackageTask([](Task &) { });
    ASSERT_TRUE(ret);
    auto &task = ret->get();
    task.setState(linglong::api::types::v1::State::Pending);
    task.setResult({ { QStringLiteral("type"), QStringLiteral("ExampleResult") },
                     { QStringLiteral("value"), 42 } });
    QObject::connect(&task, &PackageTask::TaskFinished, [&result](const QVariantMap &value) {
        result = value;
    });

    task.Cancel();
    ASSERT_TRUE(processEventsUntil([&result]() {
        return !result.empty();
    }));

    EXPECT_EQ(result.value(QStringLiteral("code")).toInt(),
              static_cast<int>(linglong::utils::error::ErrorCode::Canceled));
    EXPECT_FALSE(result.contains(QStringLiteral("value")));
}

TEST(PackageTask, jobWithoutTerminalStateFails)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    QVariantMap result;
    auto ret = queue.addPackageTask([](Task &) { });
    ASSERT_TRUE(ret);
    auto &task = ret->get();
    QObject::connect(&task, &PackageTask::TaskFinished, [&result](const QVariantMap &value) {
        result = value;
    });

    ASSERT_TRUE(processEventsUntil([&result]() {
        return !result.empty();
    }));

    EXPECT_EQ(result.value(QStringLiteral("code")).toInt(),
              static_cast<int>(linglong::utils::error::ErrorCode::Failed));
    EXPECT_EQ(result.value(QStringLiteral("message")).toString(),
              QStringLiteral("task returned without a terminal state"));
}

TEST(PackageTask, cancelBeforeStartFinishes)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    auto ret = queue.addPackageTask([](Task &) { });
    ASSERT_TRUE(ret);
    auto &task = ret->get();
    task.setState(linglong::api::types::v1::State::Pending);
    QStringList sequence;
    QObject::connect(&task,
                     &PackageTask::TaskEvent,
                     [&sequence](const QString &event, const QVariantMap &) {
                         sequence.push_back(event);
                     });
    QObject::connect(&task, &PackageTask::TaskFinished, [&sequence](const QVariantMap &) {
        sequence.push_back(QStringLiteral("finished"));
    });
    task.Cancel();

    ASSERT_TRUE(processEventsUntil([&sequence]() {
        return sequence.contains(QStringLiteral("finished"));
    }));
    EXPECT_EQ(sequence, QStringList({ QStringLiteral("state"), QStringLiteral("finished") }));
}

TEST(PackageTask, cancelWhileRunningFinishesAfterJobReturns)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);
    std::promise<void> started;
    auto startedFuture = started.get_future();
    std::promise<void> release;
    auto releaseFuture = release.get_future();
    std::atomic_bool jobReturned{ false };
    auto ret = queue.addPackageTask([&](Task &) {
        started.set_value();
        releaseFuture.wait();
        jobReturned = true;
    });
    ASSERT_TRUE(ret);
    auto &task = ret->get();

    std::promise<void> finished;
    auto finishedFuture = finished.get_future();
    QObject::connect(&task, &PackageTask::TaskFinished, [&](const QVariantMap &) {
        EXPECT_TRUE(jobReturned);
        finished.set_value();
    });

    QCoreApplication::processEvents();
    ASSERT_EQ(startedFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    task.Cancel();

    EXPECT_EQ(finishedFuture.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    release.set_value();
    EXPECT_TRUE(processEventsUntil([&finishedFuture]() {
        return finishedFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    }));
}

TEST(TaskQueue, defersDiscardUntilCancelReturns)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTaskQueue queue(nullptr);
    auto ret = queue.addPackageTask([](Task &) { });
    ASSERT_TRUE(ret);

    auto &task = ret->get();
    const auto taskID = task.taskID();
    QPointer<PackageTask> taskPointer{ &task };
    task.setState(linglong::api::types::v1::State::Pending);

    task.Cancel();

    EXPECT_FALSE(taskPointer.isNull());
    EXPECT_TRUE(queue.getTask(taskID));

    QCoreApplication::processEvents();

    EXPECT_TRUE(taskPointer.isNull());
    EXPECT_FALSE(queue.getTask(taskID));
}

TEST(TaskQueue, addTask)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTaskQueue queue(nullptr);
    std::promise<void> jobBlocked;
    auto jobBlockedFuture = jobBlocked.get_future();
    std::promise<void> release;
    auto releaseFuture = release.get_future();
    auto ret = queue.addTask([&](Task &task) {
        task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        jobBlocked.set_value();
        releaseFuture.wait();
    });
    ASSERT_TRUE(ret);
    const auto taskID = ret->get().taskID();

    QCoreApplication::processEvents();
    ASSERT_EQ(jobBlockedFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    auto task = queue.getTask(taskID);
    ASSERT_TRUE(task);
    EXPECT_EQ(task->get().state(), linglong::api::types::v1::State::Succeed);
    EXPECT_EQ(task->get().message(), "succeeded");

    release.set_value();
    for (int i = 0; i < 1000 && queue.getTask(taskID); ++i) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }

    EXPECT_FALSE(queue.getTask(taskID));
}

TEST(TaskQueue, joinTask)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    int called = 0;
    {
        PackageTaskQueue queue(nullptr);
        auto ret = queue.addTask([&app, &called](Task &task) {
            called++;
            // quit application, so next task will not be run
            app.quit();

            std::this_thread::sleep_for(std::chrono::seconds(1));

            task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        });
        EXPECT_TRUE(ret);

        ret = queue.addTask([&called](Task &task) {
            called++;
            task.updateState(linglong::api::types::v1::State::Succeed, "succeeded");
        });
        EXPECT_TRUE(ret);
        app.exec();
    }

    EXPECT_EQ(called, 1);
}

} // namespace
