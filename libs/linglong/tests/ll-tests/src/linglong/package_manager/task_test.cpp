/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package_manager/task.h"
#include "linglong/utils/log/log.h"

namespace {

using namespace linglong::service;
using ::testing::_;
using ::testing::DoubleNear;
using ::testing::InSequence;

class TaskTester : public TaskReporter
{
public:
    TaskTester(Task &task)
        : m_task(task)
    {
    }

    void onProgress() noexcept override { m_progress.emplace_back(m_task.percentage()); }

    MOCK_METHOD(void, onHandled, (uint handled, uint total), (override, noexcept));

    MOCK_METHOD(void, onStateChanged, (), (override, noexcept));

    MOCK_METHOD(void, onMessage, (), (override, noexcept));

    Task &m_task;
    std::vector<double> m_progress;
};

TEST(Task, divideParts)
{
    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    // Expect onProgress to be called with increasing values
    // We divide into 2 parts.
    // Part 1: 0% -> 50% of total.
    // Part 2: 50% -> 100% of total.
    TaskContainer container(task, { 1, 1 });

    // Get first part
    ASSERT_TRUE(container.hasNext());
    auto &sub1 = container.next();

    sub1.updateProgress(50.0);
    sub1.updateProgress(100.0);

    // Get second part
    ASSERT_TRUE(container.hasNext());
    auto &sub2 = container.next();

    sub2.updateProgress(50.0);
    sub2.updateProgress(80.0);
    sub2.updateProgress(100.0);

    ASSERT_FALSE(container.hasNext());

    EXPECT_THAT(tester.m_progress,
                ::testing::ElementsAre(DoubleNear(25.0, 0.001),
                                       DoubleNear(50.0, 0.001),
                                       DoubleNear(75.0, 0.001),
                                       DoubleNear(90.0, 0.001),
                                       DoubleNear(100.0, 0.001)));
}

TEST(Task, nestedDivideParts)
{
    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    // Divide into 3 parts.
    // Part 2 is further divided into 2 parts.

    // Part 1: 0-50%
    // Part 2: 50-75%
    //   SubPart 2.1: 50-62.5%
    //   SubPart 2.2: 62.5-75%
    // Part 3: 75-100%

    TaskContainer container1(task, { 2, 1, 1 });

    // Part 1
    container1.next();

    auto container2 = TaskContainer(container1.next(), { 1, 1 });

    // SubPart 2.1
    auto &sub2_1 = container2.next();
    sub2_1.updateProgress(50.0);
    sub2_1.updateProgress(100.0);

    // SubPart 2.2
    auto &sub2_2 = container2.next();
    sub2_2.updateProgress(50.0);
    sub2_2.updateProgress(100.0);

    // Part 3
    auto &sub3 = container1.next();
    sub3.updateProgress(50.0);
    sub3.updateProgress(100.0);

    EXPECT_THAT(tester.m_progress,
                ::testing::ElementsAre(DoubleNear(50.0, 0.001),
                                       DoubleNear(56.25, 0.001),
                                       DoubleNear(62.5, 0.001),
                                       DoubleNear(68.75, 0.001),
                                       DoubleNear(75.0, 0.001),
                                       DoubleNear(87.5, 0.001),
                                       DoubleNear(100.0, 0.001)));
}

TEST(Task, bothUpdateProgress)
{
    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    task.updateProgress(50.0);

    TaskContainer container(task, { 1, 1 });

    ASSERT_TRUE(container.hasNext());
    auto &sub1 = container.next();

    sub1.updateProgress(50.0);
    sub1.updateProgress(100.0);

    ASSERT_TRUE(container.hasNext());
    auto &sub2 = container.next();

    sub2.updateProgress(50.0);
    sub2.updateProgress(80.0);
    sub2.updateProgress(100.0);

    ASSERT_FALSE(container.hasNext());

    EXPECT_THAT(tester.m_progress,
                ::testing::ElementsAre(DoubleNear(50.0, 0.001),
                                       DoubleNear(62.5, 0.001),
                                       DoubleNear(75.0, 0.001),
                                       DoubleNear(87.5, 0.001),
                                       DoubleNear(95.0, 0.001),
                                       DoubleNear(100.0, 0.001)));
}

TEST(Task, noStepBackAndLimit)
{
    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    task.updateProgress(-10.0);
    task.updateProgress(50.0);
    task.updateProgress(30.0);
    task.updateProgress(90.0);
    task.updateProgress(110.0);

    EXPECT_THAT(tester.m_progress,
                ::testing::ElementsAre(DoubleNear(50.0, 0.001), DoubleNear(90.0, 0.001)));
}

TEST(Task, reportError)
{
    LINGLONG_TRACE("reportError");

    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    EXPECT_CALL(tester, onStateChanged).Times(1);

    task.reportError(LINGLONG_ERRV("test error"));

    EXPECT_EQ(task.state(), linglong::api::types::v1::State::Failed);
    EXPECT_EQ(task.message(), "test error");
}

TEST(Task, reportDataHandled)
{
    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    EXPECT_CALL(tester, onHandled(10, 100)).Times(1);

    task.reportDataHandled(10, 100);
}

TEST(Task, reportToOwner)
{
    LINGLONG_TRACE("reportToOwner");

    Task task;
    TaskTester tester(task);
    task.setReporter(&tester);

    TaskContainer container(task, { 1, 1 });

    auto &sub1 = container.next();
    EXPECT_CALL(tester, onStateChanged).Times(2);
    sub1.updateState(linglong::api::types::v1::State::Processing, "processing");
    EXPECT_EQ(task.state(), linglong::api::types::v1::State::Processing);
    EXPECT_EQ(task.message(), "processing");

    auto &sub2 = container.next();
    sub2.reportError(LINGLONG_ERRV("test error"));
    EXPECT_EQ(task.message(), "test error");
}

} // namespace
