// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/oci-cfg-generators/container_cfg_builder.h"

#include <common/tempdir.h>

#include <algorithm>

namespace {

class CDIGroupsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        builder.setAppId("org.example.cdi")
          .setBasePath(base.path())
          .setBundlePath(bundle.path())
          .setContainerId("cdi-groups-test")
          .disablePatch();
    }

    TempDir base{ "ll-cdi-groups-base-" };
    TempDir bundle{ "ll-cdi-groups-bundle-" };
    linglong::generator::ContainerCfgBuilder builder;
};

TEST_F(CDIGroupsTest, RejectsNonzeroGroupsBeforeApplyingOtherEdits)
{
    linglong::cdi::types::ContainerEdits edits;
    edits.additionalGids = std::vector<int64_t>{ 0, 12345 };
    edits.env = std::vector<std::string>{ "LL_CDI_GROUP_TEST=should-not-apply" };
    auto result = builder.applyCDIPatch(edits);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("additionalGids"), std::string::npos);
    EXPECT_NE(result.error().message().find("not supported"), std::string::npos);

    auto built = builder.build();
    ASSERT_TRUE(built) << built.error().message();
    ASSERT_TRUE(builder.getConfig().process);
    ASSERT_TRUE(builder.getConfig().process->env);
    for (const auto &env : *builder.getConfig().process->env) {
        EXPECT_NE(env, "LL_CDI_GROUP_TEST=should-not-apply");
    }
}

TEST_F(CDIGroupsTest, IgnoresZeroGroupsAndAppliesOtherEdits)
{
    linglong::cdi::types::ContainerEdits edits;
    edits.additionalGids = std::vector<int64_t>{ 0, 0 };
    edits.env = std::vector<std::string>{ "LL_CDI_GROUP_TEST=applied" };
    auto result = builder.applyCDIPatch(edits);
    ASSERT_TRUE(result) << result.error().message();
    auto built = builder.build();
    ASSERT_TRUE(built) << built.error().message();
    ASSERT_TRUE(builder.getConfig().process);
    ASSERT_TRUE(builder.getConfig().process->env);
    EXPECT_NE(std::find(builder.getConfig().process->env->begin(),
                        builder.getConfig().process->env->end(),
                        "LL_CDI_GROUP_TEST=applied"),
              builder.getConfig().process->env->end());
    EXPECT_FALSE(builder.getConfig().process->user);
}

TEST_F(CDIGroupsTest, AcceptsAbsentAndEmptyGroupLists)
{
    linglong::cdi::types::ContainerEdits edits;
    auto absent = builder.applyCDIPatch(edits);
    ASSERT_TRUE(absent) << absent.error().message();
    edits.additionalGids = std::vector<int64_t>{};
    auto empty = builder.applyCDIPatch(edits);
    ASSERT_TRUE(empty) << empty.error().message();
}

} // namespace
