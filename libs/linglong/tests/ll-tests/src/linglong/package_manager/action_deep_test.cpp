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
#include "linglong/utils/error/error.h"

#include <QCoreApplication>
#include <QVariantMap>

#include <memory>
#include <string>
#include <vector>

namespace {

using namespace linglong::service;

TEST(ActionDeepTest, BasicActionCreationAndExecutionFlow)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    bool executed = false;
    std::string taskMessage;

    auto taskFunc = [&executed, &taskMessage](Task &task) {
        executed = true;
        task.updateState(linglong::api::types::v1::State::Processing, "action processing");
        taskMessage = task.message().toStdString();
        task.updateState(linglong::api::types::v1::State::Succeed, "action finished successfully");
    };

    PackageTaskQueue queue(nullptr);
    auto res = queue.addPackageTask(taskFunc);
    ASSERT_TRUE(res);

    auto &task = res->get();
    bool finished = false;
    QObject::connect(&task, &PackageTask::TaskFinished, [&finished](const QVariantMap &) {
        finished = true;
    });

    while (!finished) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }

    EXPECT_TRUE(executed);
    EXPECT_EQ(taskMessage, "action processing");
    EXPECT_EQ(task.state(), linglong::api::types::v1::State::Succeed);
}

TEST(ActionDeepTest, ComplexActionChainFailureHanding)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTaskQueue queue(nullptr);
    std::vector<std::string> stepsCompleted;

    auto res = queue.addPackageTask([&stepsCompleted](Task &task) {
        stepsCompleted.push_back("step1_init");
        task.updateState(linglong::api::types::v1::State::Processing, "step 1 done");

        stepsCompleted.push_back("step2_verify");
        task.updateState(linglong::api::types::v1::State::Failed, "step 2 failed: signature error");

        stepsCompleted.push_back("step3_unreachable");
    });
    ASSERT_TRUE(res);

    auto &task = res->get();
    QVariantMap resultMap;
    QObject::connect(&task, &PackageTask::TaskFinished, [&resultMap](const QVariantMap &res) {
        resultMap = res;
    });

    while (resultMap.isEmpty()) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }

    EXPECT_EQ(stepsCompleted.size(), 3U);
    EXPECT_EQ(resultMap.value(QStringLiteral("code")).toInt(),
              static_cast<int>(linglong::utils::error::ErrorCode::Failed));
    EXPECT_EQ(resultMap.value(QStringLiteral("message")).toString(),
              QStringLiteral("step 2 failed: signature error"));
}

TEST(ActionDeepTest, ActionProgressNotificationAccumulation)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    PackageTaskQueue queue(nullptr);
    std::vector<double> progressReported;

    auto res = queue.addPackageTask([&progressReported](Task &task) {
        for (double p = 10.0; p <= 100.0; p += 30.0) {
            task.updateProgress(p, "downloading segment");
        }
        task.updateState(linglong::api::types::v1::State::Succeed, "finished download");
    });
    ASSERT_TRUE(res);

    auto &task = res->get();
    QObject::connect(
      &task,
      &PackageTask::TaskEvent,
      [&](const QString &event, const QVariantMap &data) {
          if (event == QStringLiteral("state")) {
              auto state =
                linglong::common::serialize::fromQVariantMap<linglong::api::types::v1::TaskState>(
                  data);
              if (state) {
                  progressReported.push_back(state->progress);
              }
          }
      });

    bool finished = false;
    QObject::connect(&task, &PackageTask::TaskFinished, [&finished](const QVariantMap &) {
        finished = true;
    });

    while (!finished) {
        QCoreApplication::processEvents();
        std::this_thread::yield();
    }

    ASSERT_GE(progressReported.size(), 3U);
    EXPECT_DOUBLE_EQ(progressReported.back(), 100.0);
}

} // namespace
