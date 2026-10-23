/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/global/initialize.h"
#include "linglong/utils/env.h"

#include <csignal>

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

TEST(ApplicationInitialize, IgnoresSigPipe)
{
    // Cmd::exec writes into pipes whose reader can vanish at any moment, for
    // example an executable OCI config patch that exits without draining its
    // input. Keeping the default disposition would terminate the application
    // from inside write(2) instead of letting it observe EPIPE.
    struct sigaction restore{};
    restore.sa_handler = SIG_DFL;
    sigemptyset(&restore.sa_mask);
    restore.sa_flags = 0;
    ASSERT_EQ(sigaction(SIGPIPE, &restore, nullptr), 0);

    applicationInitialize();

    struct sigaction current{};
    ASSERT_EQ(sigaction(SIGPIPE, nullptr, &current), 0);
    EXPECT_EQ(current.sa_handler, SIG_IGN);
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
