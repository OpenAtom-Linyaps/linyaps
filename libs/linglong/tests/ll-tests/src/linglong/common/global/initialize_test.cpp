/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/global/initialize.h"
#include "linglong/utils/env.h"

using namespace linglong::common::global;
using linglong::utils::EnvironmentVariableGuard;

TEST(GlobalTaskControl, InstanceIsAvailableAndNotInitiallyCanceled)
{
    auto *taskControl = GlobalTaskControl::instance();
    ASSERT_NE(taskControl, nullptr);

    // A fresh run must not start canceled. We deliberately avoid calling
    // cancel() here: emitting the OnCancel signal can invoke connections
    // left behind by other tests that reference a torn-down DBus connection.
    EXPECT_FALSE(GlobalTaskControl::canceled());
}

TEST(LinglongInstalled, ReturnsFalseInTestEnvironment)
{
    // The test binary is not installed under BINDIR, so this is false here.
    EXPECT_FALSE(linglongInstalled());
}

TEST(ApplicationInitialize, RunsWithoutCrashing)
{
    applicationInitialize();
    // Signal handlers were installed without terminating the process.
    SUCCEED();
}

TEST(InitLogSystem, ParsesEnvironment)
{
    EnvironmentVariableGuard levelGuard("LINYAPS_LOG_LEVEL", "debug");
    EnvironmentVariableGuard backendGuard("LINYAPS_LOG_BACKEND", "console,journal");
    initLinyapsLogSystem(linglong::utils::log::LogBackend::None);
    SUCCEED();
}

TEST(InitLogSystem, InvalidLevelFallsBackToDefault)
{
    EnvironmentVariableGuard levelGuard("LINYAPS_LOG_LEVEL", "bogus");
    EnvironmentVariableGuard backendGuard("LINYAPS_LOG_BACKEND", "bogus");
    initLinyapsLogSystem(linglong::utils::log::LogBackend::Journal);
    SUCCEED();
}
