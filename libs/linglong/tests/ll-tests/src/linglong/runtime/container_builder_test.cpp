// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/cli/cli.h"
#include "linglong/runtime/container_builder.h"

using namespace linglong;

TEST(RunContainerOptionsTest, ApplyRuntimeConfigEnablesDevicePassthru)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.deviceMode =
      std::vector<api::types::v1::DeviceOption>{ api::types::v1::DeviceOption::Passthru };

    auto result = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsEnablesDevicePassthru)
{
    runtime::RunContainerOptions options;
    cli::RunOptions runOptions;
    runOptions.deviceOptions = { api::types::v1::DeviceOption::Passthru };

    auto result = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}
