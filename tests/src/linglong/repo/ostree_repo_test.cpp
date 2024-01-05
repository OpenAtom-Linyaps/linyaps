/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/ref.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/file.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/yaml.h"
#include "utils/serialize/TestStruct.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstddef>
#include <iostream>
#include <memory>

namespace linglong::repo::test {

namespace {

// When testing OSTreeRepo functionality, we may initialize and commit some files into the ostree
// repository in a bash script to keep our code as unit test.
utils::error::Result<QStringList> executeTestScript(QStringList args)
{
    QProcess process;
    process.start("src/linglong/repo/ostree_repo_test.sh", args);
    if (!process.waitForFinished()) {
        return LINGLONG_ERR(
          -1,
          QString("Run ostree repo test script with %1 failed: %2")
            .arg(QString(QJsonDocument(QJsonArray::fromStringList(args)).toJson()))
            .arg(process.errorString()));
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus == 0 && retCode == 0) {
        QString lines = process.readAllStandardOutput();
        auto ret = lines.split('\n', Qt::SkipEmptyParts);
        return ret;
    }

    return LINGLONG_ERR(
      -1,
      QString("Ostree repo test script with %1 failed.\nstdout:\n%2\nstderr:\n%3")
        .arg(
          QString(QJsonDocument(QJsonArray::fromStringList(args)).toJson(QJsonDocument::Compact)))
        .arg(QString(process.readAllStandardOutput()))
        .arg(QString(process.readAllStandardError())));
}

class RepoTest : public ::testing::Test
{
protected:
    api::client::ClientApi api;
    std::unique_ptr<linglong::repo::OSTreeRepo> ostreeRepo;
    QString repoPath;
    QString ostreeRepoPath;
    QString remoteEndpoint;
    QString remoteRepoName;

    std::unique_ptr<QTemporaryDir> dir;

    void SetUp() override
    {
        dir = std::make_unique<QTemporaryDir>("repo");
        repoPath = dir->path();
        ostreeRepoPath = repoPath + "/repo";
        remoteEndpoint = "https://store-llrepo.deepin.com/repos/";
        remoteRepoName = "repo";
        auto config =
          config::ConfigV1{ remoteRepoName.toStdString(),
                            { { remoteRepoName.toStdString(), remoteEndpoint.toStdString() } },
                            1 };
        linglong::util::Connection dbConnection;
        ostreeRepo =
          std::make_unique<linglong::repo::OSTreeRepo>(repoPath, config, api, dbConnection);
    }

    void TearDown() override
    {
        ostreeRepo.reset(nullptr);
        dir.reset();
    }
};

TEST_F(RepoTest, initialize)
{
    EXPECT_TRUE(true);
}

TEST_F(RepoTest, basicMethods)
{
    auto files = executeTestScript({ "create_files", dir->path() + "/tmp" });
    if (!files.has_value()) {
        auto &err = files.error();
        qCritical() << "Create files for test failed:" << err;
        FAIL();
    }

    qDebug() << "Create temporary files" << *files;

    QString appId = "test";
    auto ret = ostreeRepo->importDirectory(package::Ref(appId), dir->path() + "/tmp");
    if (!ret.has_value()) {
        qCritical() << "Failed to import directory into ostree based linglong repository:"
                    << ret.error();
        FAIL();
    }

    QString refToCheck = QString("linglong/%1/latest/x86_64/runtime").arg(appId);
    files = executeTestScript(QStringList{ "check_commit", ostreeRepoPath, refToCheck } + *files);
    if (!files.has_value()) {
        qCritical() << "Check files at repository" << ostreeRepoPath << "failed:" << files.error();
        FAIL();
    }
    qDebug() << "Check files in ref" << refToCheck << "success";

    QDir(dir->path() + "/tmp").removeRecursively();

    ret = ostreeRepo->checkoutAll(package::Ref(appId), "", dir->path() + "/tmp");
    if (!ret.has_value()) {
        qCritical() << "Checkout reference" << appId << "failed:" << ret.error();
        FAIL();
    }

    files->push_front("check_files");
    files = executeTestScript(*files);
    if (!files.has_value()) {
        qCritical() << "Check files failed:" << files.error();
        FAIL();
    }

    auto ref = ostreeRepo->localLatestRef(package::Ref(appId));
    if (!ref.has_value()) {
        qCritical() << "Get local latest reference of application" << appId
                    << "failed:" << ref.error();
        FAIL();
    }
    EXPECT_EQ(ref->appId, appId);

    ret = ostreeRepo->repoDeleteDatabyRef(repoPath, package::Ref("test").toOSTreeRefLocalString());
    if (!ret.has_value()) {
        qCritical() << "Delete" << appId << "from repository failed:" << ret.error();
        FAIL();
    }

    ref = ostreeRepo->localLatestRef(package::Ref(appId));
    if (ref.has_value()) {
        qCritical() << "Reference" << appId << "should be deleted";
        FAIL();
    }
}

TEST_F(RepoTest, pull)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, remoteShowUrl)
{
    auto ret = ostreeRepo->remoteShowUrl(remoteRepoName);
    if (!ret.has_value()) {
        qCritical() << ret.error();
        FAIL();
    }
}

TEST_F(RepoTest, rootOfLayer)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, latestOfRef)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, remoteLatestRef)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, getRemoteRepoList)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, getRemoteRefs)
{
    GTEST_SKIP();
}

} // namespace
} // namespace linglong::repo::test
