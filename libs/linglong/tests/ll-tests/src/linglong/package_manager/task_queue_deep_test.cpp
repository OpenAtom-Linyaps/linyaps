/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/serialize/json.h"
#include "linglong/package_manager/action.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/utils/log/log.h"

#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace linglong::service;
using ::testing::_;

template <typename Predicate>
bool waitForCondition(Predicate &&predicate,
                      std::chrono::milliseconds timeout = std::chrono::seconds(2))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }
    return predicate();
}

TEST(PackageTaskDeepTest, MultiTaskConcurrencyAndQueueOrdering)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    constexpr int kTaskCount = 5;
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    std::vector<std::shared_ptr<std::promise<void>>> releasePromises;

    for (int i = 0; i < kTaskCount; ++i) {
        auto promise = std::make_shared<std::promise<void>>();
        releasePromises.push_back(promise);

        auto res = queue.addPackageTask([i, promise, &executionOrder, &orderMutex](Task &task) {
            promise->get_future().wait();
            {
                std::lock_guard<std::mutex> lock(orderMutex);
                executionOrder.push_back(i);
            }
            task.updateState(linglong::api::types::v1::State::Succeed,
                             "finished task " + std::to_string(i));
        });
        ASSERT_TRUE(res);
    }

    for (int i = 0; i < kTaskCount; ++i) {
        releasePromises[i]->set_value();
        QCoreApplication::processEvents();
    }

    ASSERT_TRUE(waitForCondition([&executionOrder]() {
        return executionOrder.size() == kTaskCount;
    }));

    for (int i = 0; i < kTaskCount; ++i) {
        EXPECT_EQ(executionOrder[i], i);
    }
}

TEST(PackageTaskDeepTest, TaskStateTransitionSequenceValidation)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    std::vector<linglong::api::types::v1::State> observedStates;

    auto res = queue.addPackageTask([&observedStates](Task &task) {
        task.updateState(linglong::api::types::v1::State::Pending, "pending stage");
        task.updateState(linglong::api::types::v1::State::Downloading, "downloading files");
        task.updateProgress(50, "halfway");
        task.updateState(linglong::api::types::v1::State::Processing, "unpacking layers");
        task.updateProgress(90, "almost done");
        task.updateState(linglong::api::types::v1::State::Succeed, "completed");
    });
    ASSERT_TRUE(res);

    auto &task = res->get();
    QObject::connect(
      &task,
      &PackageTask::TaskEvent,
      [&observedStates](const QString &event, const QVariantMap &data) {
          if (event == QStringLiteral("state")) {
              auto st =
                linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
                  data);
              if (st) {
                  observedStates.push_back(st->state);
              }
          }
      });

    bool finished = false;
    QObject::connect(&task, &PackageTask::TaskFinished, [&finished](const QVariantMap &) {
        finished = true;
    });

    ASSERT_TRUE(waitForCondition([&finished]() {
        return finished;
    }));
    ASSERT_GE(observedStates.size(), 4U);
    EXPECT_EQ(observedStates.back(), linglong::api::types::v1::State::Succeed);
}

TEST(PackageTaskDeepTest, InteractionTimeoutAndRejection)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTask task({});
    task.setState(linglong::api::types::v1::State::Processing);

    int interactionCount = 0;
    QObject::connect(&task,
                     &PackageTask::RequestInteraction,
                     [&](const QString &id, int, const QVariantMap &) {
                         interactionCount++;
                         task.ReplyInteraction(
                           id,
                           linglong::common::serialize::toQVariantMap(
                             linglong::api::types::v1::InteractionReply{ .action = "cancel" }));
                     });

    bool ok = task.requestInteraction(
      linglong::api::types::v1::InteractionMessageType::Uninstall,
      linglong::api::types::v1::PackageManager1RequestInteractionAdditionalMessage{});

    EXPECT_FALSE(ok);
    EXPECT_EQ(interactionCount, 1);
}

TEST(PackageTaskDeepTest, ParallelTaskCancellationResilience)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    constexpr int kTotal = 6;
    std::atomic<int> completedCount{ 0 };
    std::atomic<int> canceledCount{ 0 };

    for (int i = 0; i < kTotal; ++i) {
        auto res = queue.addPackageTask([i, &completedCount](Task &task) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            task.updateState(linglong::api::types::v1::State::Succeed, "ok");
            completedCount++;
        });
        ASSERT_TRUE(res);

        if (i % 2 == 1) {
            res->get().Cancel();
            canceledCount++;
        }
    }

    ASSERT_TRUE(waitForCondition(
      [&completedCount, &canceledCount]() {
          return (completedCount + canceledCount) == kTotal;
      },
      std::chrono::seconds(5)));

    EXPECT_EQ(canceledCount.load(), 3);
}

TEST(PackageTaskDeepTest, RapidTaskSubmissionAndDestructionSafety)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    {
        PackageTaskQueue localQueue(nullptr);
        for (int i = 0; i < 20; ++i) {
            localQueue.addPackageTask([](Task &task) {
                task.updateState(linglong::api::types::v1::State::Succeed, "fast");
            });
        }
    }
    // Destruction of localQueue should not double-free or crash pending async jobs
    QCoreApplication::processEvents();
    SUCCEED();
}

TEST(PackageTaskDeepTest, ExtendedProgressMetricsAndMessageFormatting)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    std::vector<double> progressTrail;
    std::vector<std::string> messageTrail;

    auto res = queue.addPackageTask([&](Task &task) {
        for (int p = 0; p <= 100; p += 25) {
            task.updateProgress(static_cast<double>(p), "step " + std::to_string(p));
        }
        task.updateState(linglong::api::types::v1::State::Succeed, "finished");
    });
    ASSERT_TRUE(res);

    auto &task = res->get();
    QObject::connect(
      &task,
      &PackageTask::TaskEvent,
      [&](const QString &event, const QVariantMap &data) {
          if (event == QStringLiteral("state")) {
              auto st =
                linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
                  data);
              if (st) {
                  progressTrail.push_back(st->progress);
                  messageTrail.push_back(st->message.toStdString());
              }
          }
      });

    bool done = false;
    QObject::connect(&task, &PackageTask::TaskFinished, [&](const QVariantMap &) {
        done = true;
    });

    ASSERT_TRUE(waitForCondition([&done]() {
        return done;
    }));
    ASSERT_FALSE(progressTrail.empty());
    EXPECT_DOUBLE_EQ(progressTrail.back(), 100.0);
}

TEST(PackageTaskDeepTest, ErrorPropagationAndCodeTranslation)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    QVariantMap finalResult;
    auto res = queue.addPackageTask([](Task &task) {
        task.updateState(linglong::api::types::v1::State::Failed, "disk error encountered");
    });
    ASSERT_TRUE(res);

    auto &task = res->get();
    QObject::connect(&task, &PackageTask::TaskFinished, [&](const QVariantMap &resMap) {
        finalResult = resMap;
    });

    ASSERT_TRUE(waitForCondition([&finalResult]() {
        return !finalResult.isEmpty();
    }));
    EXPECT_EQ(finalResult.value(QStringLiteral("code")).toInt(),
              static_cast<int>(linglong::utils::error::ErrorCode::Failed));
    EXPECT_EQ(finalResult.value(QStringLiteral("message")).toString(),
              QStringLiteral("disk error encountered"));
}

TEST(PackageTaskDeepTest, HeavyStressTaskChurnAndLookupConsistency)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    PackageTaskQueue queue(nullptr);

    std::vector<QString> taskIDs;
    for (int i = 0; i < 15; ++i) {
        auto res = queue.addPackageTask([i](Task &task) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            task.updateState(linglong::api::types::v1::State::Succeed, "task " + std::to_string(i));
        });
        ASSERT_TRUE(res);
        taskIDs.push_back(res->get().taskID());
    }

    for (const auto &id : taskIDs) {
        auto handle = queue.getTask(id);
        // Handle might be valid or expired depending on process completion
        if (handle) {
            EXPECT_EQ(handle->get().taskID(), id);
        }
    }

    ASSERT_TRUE(waitForCondition(
      [&queue, &taskIDs]() {
          for (const auto &id : taskIDs) {
              if (queue.getTask(id))
                  return false;
          }
          return true;
      },
      std::chrono::seconds(5)));
}

} // namespace
