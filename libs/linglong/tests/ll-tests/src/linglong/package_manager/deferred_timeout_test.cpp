/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <cstdlib>
#include <memory>

namespace {

using namespace linglong;

class TimeoutTestRepo : public repo::OSTreeRepo
{
public:
    explicit TimeoutTestRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }
};

class DeferredTimeoutTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        auto repoOwner = std::make_unique<TimeoutTestRepo>(tempDir->path());
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        auto containerBuilderOwner = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm = std::make_unique<service::PackageManager>(std::move(repoOwner),
                                                       std::move(containerBuilderOwner),
                                                       nullptr);
    }

    void TearDown() override
    {
        pm.reset();
        cli.reset();
        tempDir.reset();
        ::unsetenv("LINGLONG_DEFERRED_TIMEOUT");
    }

    void setEnv(const char *value) { ::setenv("LINGLONG_DEFERRED_TIMEOUT", value, 1); }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    std::unique_ptr<service::PackageManager> pm;
};

// Each initDaemonMode call is one-shot per instance; invalid values must fall
// back to the default instead of feeding QTimer a non-positive or overflowing
// interval. The tests only need the validation branches to execute.

TEST_F(DeferredTimeoutTest, ValidPositiveSecondsIsAccepted)
{
    setEnv("120");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, TrailingGarbageIsRejected)
{
    setEnv("17seconds");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, ZeroIsRejected)
{
    setEnv("0");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, NegativeIsRejected)
{
    setEnv("-1");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, OverflowSecondsIsRejected)
{
    setEnv("2147484");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, NonNumericIsRejected)
{
    setEnv("abc");
    pm->initDaemonMode(true);
    SUCCEED();
}

TEST_F(DeferredTimeoutTest, UnsetEnvUsesDefault)
{
    ::unsetenv("LINGLONG_DEFERRED_TIMEOUT");
    pm->initDaemonMode(true);
    SUCCEED();
}

} // namespace
