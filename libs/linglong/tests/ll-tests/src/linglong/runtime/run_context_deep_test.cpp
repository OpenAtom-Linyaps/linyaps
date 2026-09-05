/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/cli/cli.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"

#include <QDir>
#include <QTemporaryDir>

#include <string>
#include <vector>

namespace linglong::runtime {

class RunContextDeepTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());
        workDir = tempDir.path().toStdString();
    }

    QTemporaryDir tempDir;
    std::string workDir;
};

TEST_F(RunContextDeepTest, RunContextConfigDefaultInitialization)
{
    api::types::v1::RunContextConfig cfg;
    EXPECT_TRUE(cfg.version.empty());
    EXPECT_TRUE(cfg.base.empty());
    EXPECT_TRUE(cfg.runtime.empty());
}

TEST_F(RunContextDeepTest, ContainerIDHashingConsistency)
{
    api::types::v1::RunContextConfig cfg;
    cfg.version = "1.0.0";
    cfg.base = "org.deepin.base/23.0.0/x86_64";
    cfg.runtime = "org.deepin.runtime/23.0.0/x86_64";

    std::string id1 = genContainerID(cfg);
    std::string id2 = genContainerID(cfg);

    EXPECT_FALSE(id1.empty());
    EXPECT_EQ(id1, id2);
}

TEST_F(RunContextDeepTest, RunContextConfigVersionComparison)
{
    api::types::v1::RunContextConfig cfgA;
    cfgA.version = "1.0.0";
    cfgA.base = "org.deepin.base/23.0.0/x86_64";

    api::types::v1::RunContextConfig cfgB;
    cfgB.version = "1.0.1";
    cfgB.base = "org.deepin.base/23.0.0/x86_64";

    EXPECT_NE(genContainerID(cfgA), genContainerID(cfgB));
}

TEST_F(RunContextDeepTest, RunContextConfigArchitectureVariations)
{
    api::types::v1::RunContextConfig cfgX86;
    cfgX86.base = "org.deepin.base/23.0.0/x86_64";

    api::types::v1::RunContextConfig cfgArm;
    cfgArm.base = "org.deepin.base/23.0.0/aarch64";

    EXPECT_NE(genContainerID(cfgX86), genContainerID(cfgArm));
}

TEST_F(RunContextDeepTest, MultiConfigHashCollisionResistance)
{
    std::vector<api::types::v1::RunContextConfig> configs;
    for (int i = 0; i < 10; ++i) {
        api::types::v1::RunContextConfig c;
        c.version = "1.0." + std::to_string(i);
        c.base = "org.deepin.base/23.0.0/x86_64";
        c.runtime = "org.deepin.runtime/23.0.0/x86_64";
        configs.push_back(c);
    }

    std::vector<std::string> hashes;
    for (const auto &c : configs) {
        hashes.push_back(genContainerID(c));
    }

    for (size_t i = 0; i < hashes.size(); ++i) {
        for (size_t j = i + 1; j < hashes.size(); ++j) {
            EXPECT_NE(hashes[i], hashes[j]);
        }
    }
}

} // namespace linglong::runtime
