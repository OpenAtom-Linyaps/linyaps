/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/crun/Crun.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Process.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace {

using linglong::runtime::Container;
using linglong::runtime::ContainerContext;
using linglong::runtime::RunContext;
using linglong::utils::EnvironmentVariableGuard;

class DummyRepo : public linglong::repo::OSTreeRepo
{
public:
    DummyRepo(const fs::path &path)
        : linglong::repo::OSTreeRepo(path,
                                     linglong::api::types::v1::RepoConfigV2{
                                       .defaultRepo = "",
                                       .repos = {},
                                       .version = 2,
                                     })
    {
    }
};

class ContainerTestRunContext final : public RunContext
{
public:
    explicit ContainerTestRunContext(linglong::repo::OSTreeRepo &r)
        : RunContext(r)
    {
    }

protected:
    auto selectOverlayMode(linglong::utils::OverlayMode requestedMode) const
      -> linglong::utils::error::Result<linglong::utils::OverlayMode> override
    {
        return requestedMode;
    }
};

ocppi::runtime::config::types::Process makeProcess()
{
    ocppi::runtime::config::types::Process process;
    process.args = std::vector<std::string>{ "echo", "hello" };
    process.cwd = "/";
    return process;
}

ocppi::runtime::config::types::Config makeConfig()
{
    ocppi::runtime::config::types::Config cfg;
    cfg.ociVersion = "1.0.2";
    cfg.process = makeProcess();
    cfg.mounts = std::vector<ocppi::runtime::config::types::Mount>{};
    return cfg;
}

class ContainerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        temp_dir = std::make_unique<TempDir>();
        env_guard =
          std::make_unique<EnvironmentVariableGuard>("XDG_RUNTIME_DIR", temp_dir->path().string());
        repo = std::make_unique<DummyRepo>(temp_dir->path());
        context = std::make_unique<ContainerTestRunContext>(*repo);
    }

    std::unique_ptr<TempDir> temp_dir;
    std::unique_ptr<EnvironmentVariableGuard> env_guard;
    std::unique_ptr<DummyRepo> repo;
    std::unique_ptr<ContainerTestRunContext> context;
};

TEST_F(ContainerTest, CreateMakesBundleDirectoryAndRootfs)
{
    auto result =
      ContainerContext::create(*context,
                               ContainerContext::CreateOptions{ .bundleSuffix = "-test" });
    ASSERT_TRUE(result.has_value());

    const auto &ctx = *result;
    EXPECT_EQ(ctx->getContainerID(), "");
    EXPECT_EQ(ctx->getBundleDir(), temp_dir->path() / "linglong" / "-test");
    EXPECT_FALSE(ctx->getContainerCache().has_value());
    EXPECT_TRUE(fs::is_directory(ctx->getBundleDir() / "rootfs"));
}

TEST_F(ContainerTest, CreateWithAppCacheCreatesCacheDirectory)
{
    auto cache = temp_dir->path() / "app-cache";
    auto result =
      ContainerContext::create(*context, ContainerContext::CreateOptions{ .appCache = cache });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)->getContainerCache(), cache);
    EXPECT_TRUE(fs::is_directory(cache));
}

TEST_F(ContainerTest, GenLdConfWithOverlayWritesIntoRootfs)
{
    auto result =
      ContainerContext::create(*context, ContainerContext::CreateOptions{ .bundleSuffix = "-ld" });
    ASSERT_TRUE(result.has_value());

    auto gen = (*result)->genLdConf("custom ld config", true);
    ASSERT_TRUE(gen.has_value());

    auto confPath =
      (*result)->getBundleDir() / "rootfs" / "etc" / "ld.so.conf.d" / "zz_deepin-linglong-app.conf";
    EXPECT_TRUE(fs::exists(confPath));
}

TEST_F(ContainerTest, GenLdConfWithoutOverlayWritesIntoAppCache)
{
    auto cache = temp_dir->path() / "cache";
    auto result =
      ContainerContext::create(*context, ContainerContext::CreateOptions{ .appCache = cache });
    ASSERT_TRUE(result.has_value());

    auto gen = (*result)->genLdConf("cache ld config", false);
    ASSERT_TRUE(gen.has_value());
    EXPECT_TRUE(fs::exists(cache / "ld.so.conf"));
}

TEST_F(ContainerTest, GenLdConfWithoutOverlayFailsWhenCacheMissing)
{
    auto result =
      ContainerContext::create(*context, ContainerContext::CreateOptions{ .bundleSuffix = "-n" });
    ASSERT_TRUE(result.has_value());

    auto gen = (*result)->genLdConf("content", false);
    EXPECT_FALSE(gen.has_value());
    EXPECT_THAT(gen.error().message(), testing::HasSubstr("container cache is empty"));
}

TEST_F(ContainerTest, RunWritesEntrypointAndConfigAndInvokesRuntime)
{
    auto runtimePath = temp_dir->path() / "runtime";
    {
        std::ofstream runtime(runtimePath);
        runtime << "#!/bin/sh\n"
                   "exit 0\n";
    }
    fs::permissions(runtimePath, fs::perms::owner_all, fs::perm_options::replace);

    auto cli = ocppi::cli::crun::Crun::New(runtimePath);
    ASSERT_TRUE(cli.has_value());

    auto createResult =
      ContainerContext::create(*context, ContainerContext::CreateOptions{ .bundleSuffix = "-run" });
    ASSERT_TRUE(createResult.has_value());

    auto cfg = makeConfig();
    Container container(std::move(cfg), std::move(*createResult), **cli);

    ocppi::runtime::RunOption option;
    auto result = container.run(makeProcess(), option);

    ASSERT_TRUE(result.has_value());
    auto bundleDir = container.bundle();
    EXPECT_TRUE(fs::exists(bundleDir / "entrypoint.sh"));
    EXPECT_TRUE(fs::exists(bundleDir / "config.json"));
}

TEST_F(ContainerTest, RunReportsRuntimeFailure)
{
    auto runtimePath = temp_dir->path() / "runtime-fail";
    {
        std::ofstream runtime(runtimePath);
        runtime << "#!/bin/sh\n"
                   "exit 42\n";
    }
    fs::permissions(runtimePath, fs::perms::owner_all, fs::perm_options::replace);

    auto cli = ocppi::cli::crun::Crun::New(runtimePath);
    ASSERT_TRUE(cli.has_value());

    auto createResult =
      ContainerContext::create(*context,
                               ContainerContext::CreateOptions{ .bundleSuffix = "-fail" });
    ASSERT_TRUE(createResult.has_value());

    auto cfg = makeConfig();
    Container container(std::move(cfg), std::move(*createResult), **cli);

    ocppi::runtime::RunOption option;
    auto result = container.run(makeProcess(), option);

    EXPECT_FALSE(result.has_value());
}

} // namespace
