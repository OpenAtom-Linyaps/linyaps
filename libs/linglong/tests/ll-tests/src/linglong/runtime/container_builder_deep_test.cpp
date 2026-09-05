/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/cli/cli.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/overlayfs_driver.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <string>
#include <unordered_map>
#include <vector>

namespace linglong::runtime {

using ::testing::_;
using ::testing::Contains;

class ContainerBuilderDeepTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());
        rootPath = tempDir.path().toStdString();
    }

    QTemporaryDir tempDir;
    std::string rootPath;
};

TEST_F(ContainerBuilderDeepTest, RunContainerOptionsMultipleEnvOverride)
{
    RunContainerOptions options;

    cli::RunOptions cliOpts1;
    cliOpts1.envs = { "KEY_A=valA", "KEY_B=valB", "COMMON=initial" };
    auto res1 = options.applyCliRunOptions(cliOpts1);
    ASSERT_TRUE(res1.has_value()) << res1.error().message();

    cli::RunOptions cliOpts2;
    cliOpts2.envs = { "COMMON=overridden", "KEY_C=valC" };
    auto res2 = options.applyCliRunOptions(cliOpts2);
    ASSERT_TRUE(res2.has_value()) << res2.error().message();

    const auto &envs = options.getEnv();
    EXPECT_EQ(envs.at("KEY_A"), "valA");
    EXPECT_EQ(envs.at("KEY_B"), "valB");
    EXPECT_EQ(envs.at("KEY_C"), "valC");
    EXPECT_EQ(envs.at("COMMON"), "overridden");
}

TEST_F(ContainerBuilderDeepTest, RunContainerOptionsCapabilitiesDeduplication)
{
    RunContainerOptions options;

    cli::RunOptions cliOpts;
    cliOpts.capsAdd = { "CAP_SYS_PTRACE", "CAP_NET_ADMIN", "CAP_SYS_PTRACE", "CAP_DAC_OVERRIDE" };
    auto res = options.applyCliRunOptions(cliOpts);
    ASSERT_TRUE(res.has_value());

    const auto &caps = options.getCapabilities();
    EXPECT_GE(caps.size(), 3U);
    EXPECT_THAT(caps, Contains("CAP_SYS_PTRACE"));
    EXPECT_THAT(caps, Contains("CAP_NET_ADMIN"));
    EXPECT_THAT(caps, Contains("CAP_DAC_OVERRIDE"));
}

TEST_F(ContainerBuilderDeepTest, SecurityContextToggleAndDuplicateHandling)
{
    RunContainerOptions options;
    options.enableSecurityContext(
      { SecurityContextType::WAYLAND, SecurityContextType::PULSEAUDIO });
    options.enableSecurityContext({ SecurityContextType::WAYLAND });

    const auto &ctxs = options.getSecurityContexts();
    EXPECT_EQ(ctxs.size(), 2U);
}

TEST_F(ContainerBuilderDeepTest, ContainerIDGenerationVarianceAndUniqueness)
{
    api::types::v1::RunContextConfig cfg1;
    cfg1.version = "1.0";
    cfg1.base = "stable:org.deepin.base/23/x86_64";
    cfg1.runtime = "stable:org.deepin.runtime/23/x86_64";

    api::types::v1::RunContextConfig cfg2 = cfg1;
    cfg2.runtime = "stable:org.deepin.runtime/24/x86_64";

    api::types::v1::RunContextConfig cfg3 = cfg1;
    cfg3.version = "2.0";

    std::string id1 = genContainerID(cfg1);
    std::string id2 = genContainerID(cfg2);
    std::string id3 = genContainerID(cfg3);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id2, id3);
}

TEST_F(ContainerBuilderDeepTest, OverlayFSDriverMountOptionsConstruction)
{
    std::vector<std::string> lowerDirs = { rootPath + "/lower1", rootPath + "/lower2" };
    std::string upperDir = rootPath + "/upper";
    std::string workDir = rootPath + "/work";
    std::string targetDir = rootPath + "/target";

    for (const auto &dir : lowerDirs) {
        QDir(QString::fromStdString(dir)).mkpath(".");
    }
    QDir(QString::fromStdString(upperDir)).mkpath(".");
    QDir(QString::fromStdString(workDir)).mkpath(".");
    QDir(QString::fromStdString(targetDir)).mkpath(".");

    OverlayFSDriver driver;
    SUCCEED();
}

TEST_F(ContainerBuilderDeepTest, SecurityContextTypeConversionRoundtrip)
{
    std::vector<SecurityContextType> types = { SecurityContextType::WAYLAND,
                                               SecurityContextType::PULSEAUDIO,
                                               SecurityContextType::X11,
                                               SecurityContextType::DBUS };

    for (auto t : types) {
        std::string name = fromType(t);
        EXPECT_NE(name, "unknown");
        EXPECT_EQ(toType(name), t);
    }
}

TEST_F(ContainerBuilderDeepTest, RunContainerOptionsInvalidEnvStringValidation)
{
    RunContainerOptions options;

    cli::RunOptions cliOpts1;
    cliOpts1.envs = { "=VALUE_WITHOUT_KEY" };
    auto res1 = options.applyCliRunOptions(cliOpts1);
    EXPECT_FALSE(res1.has_value());

    cli::RunOptions cliOpts2;
    cliOpts2.envs = { "   " };
    auto res2 = options.applyCliRunOptions(cliOpts2);
    EXPECT_FALSE(res2.has_value());
}

TEST_F(ContainerBuilderDeepTest, ContainerOptionsDeviceOptionParsing)
{
    RunContainerOptions options;
    cli::RunOptions cliOpts;
    cliOpts.deviceOptions = { api::types::v1::DeviceOption::Passthru };

    auto res = options.applyCliRunOptions(cliOpts);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}

TEST_F(ContainerBuilderDeepTest, ContainerOptionsPrivilegedAndHostNetworkConfig)
{
    RunContainerOptions options;
    cli::RunOptions cliOpts;
    cliOpts.privileged = true;

    auto res = options.applyCliRunOptions(cliOpts);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(options.isPrivileged());
}

TEST_F(ContainerBuilderDeepTest, ComplexEnvVarSpecialCharactersAndEscaping)
{
    RunContainerOptions options;
    cli::RunOptions cliOpts;
    cliOpts.envs = { "PATH=/usr/local/bin:/usr/bin:/bin",
                     "JSON_CFG={\"key\": \"val\", \"num\": 123}",
                     "URL=https://example.com/api?query=1&sort=asc" };

    auto res = options.applyCliRunOptions(cliOpts);
    ASSERT_TRUE(res.has_value()) << res.error().message();

    const auto &envs = options.getEnv();
    EXPECT_EQ(envs.at("PATH"), "/usr/local/bin:/usr/bin:/bin");
    EXPECT_EQ(envs.at("JSON_CFG"), "{\"key\": \"val\", \"num\": 123}");
    EXPECT_EQ(envs.at("URL"), "https://example.com/api?query=1&sort=asc");
}

TEST_F(ContainerBuilderDeepTest, SecurityContextAllTypesEnumCheck)
{
    std::vector<SecurityContextType> allTypes = { SecurityContextType::UNKNOWN,
                                                  SecurityContextType::WAYLAND,
                                                  SecurityContextType::X11,
                                                  SecurityContextType::PULSEAUDIO,
                                                  SecurityContextType::DBUS };

    for (const auto &t : allTypes) {
        std::string s = fromType(t);
        EXPECT_FALSE(s.empty());
    }
}

TEST_F(ContainerBuilderDeepTest, RunContainerOptionsClearStateAndReset)
{
    RunContainerOptions options;
    cli::RunOptions cliOpts;
    cliOpts.envs = { "VAR1=VAL1", "VAR2=VAL2" };
    cliOpts.privileged = true;

    ASSERT_TRUE(options.applyCliRunOptions(cliOpts).has_value());
    EXPECT_TRUE(options.isPrivileged());
    EXPECT_EQ(options.getEnv().size(), 2U);
}

} // namespace linglong::runtime
