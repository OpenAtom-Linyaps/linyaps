/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/uab_installation.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>

#include <memory>

#include <fcntl.h>

namespace {

using namespace linglong;

class TestRepo : public repo::OSTreeRepo
{
public:
    explicit TestRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
          path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }
};

class PackageManagerCacheTest : public ::testing::Test
{
protected:
    auto stageInstallFile(const QByteArray &payload, const QString &fileType)
      -> std::shared_ptr<service::CachedInstallFile>
    {
        const auto sourcePath = sourceDir.path() / ("demo." + fileType.toStdString());
        QFile sourceFile(QString::fromStdString(sourcePath.string()));
        EXPECT_TRUE(sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        EXPECT_EQ(sourceFile.write(payload), payload.size());
        sourceFile.close();

        const auto rawFd = ::open(sourcePath.c_str(), O_RDONLY | O_CLOEXEC);
        EXPECT_GE(rawFd, 0);

        QDBusUnixFileDescriptor fd(rawFd);
        auto stagedFile = service::PackageManager::stageInstallFile(fd, fileType, cacheDir.path());
        EXPECT_TRUE(stagedFile);
        return *stagedFile;
    }

    void SetUp() override
    {
        auto repoOwner = std::make_unique<TestRepo>(cacheDir.path());
        repo = repoOwner.get();
        cli = ocppi::cli::crun::Crun::New(cacheDir.path()).value();
        auto containerBuilderOwner = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm = std::make_unique<service::PackageManager>(std::move(repoOwner),
                                                       std::move(containerBuilderOwner),
                                                       nullptr);
    }

    void TearDown() override
    {
        pm.reset();
        cli.reset();
    }

    TempDir sourceDir;
    TempDir cacheDir;
    repo::OSTreeRepo *repo{ nullptr };
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    std::unique_ptr<service::PackageManager> pm;
};

TEST_F(PackageManagerCacheTest, StageInstallFileCopiesPayloadIntoCacheDirectory)
{
    const QByteArray payload("install-from-file-cache-copy");
    auto stagedFile = stageInstallFile(payload, "uab");

    EXPECT_TRUE(QFileInfo::exists(QString::fromStdString(stagedFile->path())));
    EXPECT_EQ(QFileInfo(QString::fromStdString(stagedFile->path())).absolutePath(),
              QString::fromStdString(cacheDir.path().string()));

    QFile copiedFile(QString::fromStdString(stagedFile->path()));
    ASSERT_TRUE(copiedFile.open(QIODevice::ReadOnly));
    EXPECT_EQ(copiedFile.readAll(), payload);
}

TEST_F(PackageManagerCacheTest, CachedInstallFileRemovesStagedCopyOnDestruction)
{
    const QByteArray payload("install-from-file-cache-copy");
    auto stagedFile = stageInstallFile(payload, "uab");
    const auto stagedPath = stagedFile->path();

    ASSERT_TRUE(QFileInfo::exists(QString::fromStdString(stagedPath)));
    stagedFile.reset();

    EXPECT_FALSE(QFileInfo::exists(QString::fromStdString(stagedPath)));
}

TEST_F(PackageManagerCacheTest, UabInstallationActionKeepsStagedFileUntilActionDestruction)
{
    const QByteArray payload("install-from-file-cache-copy");
    auto stagedFile = stageInstallFile(payload, "uab");
    const auto stagedPath = stagedFile->path();

    auto action = service::UabInstallationAction::create(stagedFile, *pm, *repo, {});

    stagedFile.reset();
    ASSERT_TRUE(QFileInfo::exists(QString::fromStdString(stagedPath)));

    action.reset();

    EXPECT_FALSE(QFileInfo::exists(QString::fromStdString(stagedPath)));
}

} // namespace
