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

#include <qdir.h>
#include <qprocess.h>
#include <qstandardpaths.h>

#include <QDir>

#include <cstddef>
#include <iostream>
#include <memory>

namespace linglong::repo::test {

namespace {

int executeTestScript(QString func, QStringList args)
{
    QProcess process;
    QStringList arguments = { QDir::currentPath() + "/src/linglong/repo/ostree_repo_test.sh",
                              func };
    arguments.append(args);
    qInfo() << arguments;
    process.start("/bin/bash", arguments);
    if (!process.waitForStarted()) {
        std::cerr << "failed to start script function: " << func.toStdString().c_str() << std::endl;
        return -1;
    }
    if (!process.waitForFinished()) {
        std::cerr << "run script function unfinished: " << func.toStdString().c_str() << std::endl;
        return -1;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus != 0 || retCode != 0) {
        std::cerr << "run script function failed" << std::endl;
        return -1;
    }
    QString output = process.readAllStandardOutput();
    qInfo() << output;
    return 0;
}

class RepoTest : public ::testing::Test
{
protected:
    api::client::ClientApi api;
    std::unique_ptr<linglong::repo::OSTreeRepo> ostreeRepo;
    QString repoPath;
    QString commitDir;
    QString commitFile;
    QString testCommitPath;

    void SetUp() override
    {
        commitDir = "commit";
        commitFile = "test_file.txt";
        testCommitPath = QStringList{ "commit", "test_file.txt" }.join(QDir::separator());
        repoPath = "repo";
        std::cout << repoPath.toStdString().c_str() << std::endl;
        ostreeRepo = std::make_unique<linglong::repo::OSTreeRepo>(repoPath, "", "", api);
    }

    void TearDown() override { QDir(repoPath).removeRecursively(); }
};

class RemoteRepoTest : public ::testing::Test
{
protected:
    api::client::ClientApi api;
    std::unique_ptr<linglong::repo::OSTreeRepo> ostreeRepo;
    QString localRepoPath;
    QString remoteEndpoint;
    QString remoteRepoName;
    package::Ref testRef = package::Ref("aom");

    void SetUp() override
    {
        localRepoPath = "repo";
        remoteEndpoint = "https://mirror-repo-linglong.deepin.com";
        remoteRepoName = "remote-repo";
        ostreeRepo = std::make_unique<linglong::repo::OSTreeRepo>(localRepoPath,
                                                                  remoteEndpoint,
                                                                  remoteRepoName,
                                                                  api);
        ostreeRepo->remoteAdd(remoteRepoName, "https://store-llrepo.deepin.com/repos/repo");
    }

    void TearDown() override { QDir(localRepoPath).removeRecursively(); }
};

TEST_F(RepoTest, remoteAdd)
{
    QString remoteUrl = "https://ostree.test";
    auto ret = ostreeRepo->remoteAdd(repoPath, remoteUrl);
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    auto result =
      executeTestScript("test_remoteadd", QStringList{ repoPath + "/" + "repo", repoPath });
    EXPECT_EQ(result, 0);
}

TEST_F(RepoTest, importDirectory)
{
    ostreeRepo->init(repoPath);
    util::ensureDir("commit_test");
    util::ensureDir(QStringList{ "commit_test", "commit" }.join(QDir::separator()));
    QFile file("commit_test/commit/test_file.txt");
    bool ok = file.open(QIODevice::ReadWrite);
    EXPECT_EQ(ok, true);
    QString appId = "test";
    auto ret = ostreeRepo->importDirectory(package::Ref(appId), "commit_test");
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    QString refs = QString("linglong/%1/latest/x86_64/runtime").arg(appId);
    auto result = executeTestScript("test_importdirectory",
                                    QStringList{ repoPath + "/" + "repo", refs, testCommitPath });
    EXPECT_EQ(result, 0);
    QDir dir("commit_test");
    dir.removeRecursively();
}

TEST_F(RepoTest, checkout)
{
    auto ret = ostreeRepo->checkout(package::Ref("test"), commitDir, QDir::currentPath());
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    QString filePath = QDir::currentPath() + "/" + commitFile;
    auto result = executeTestScript("test_checkout", QStringList{ filePath });
    EXPECT_EQ(result, 0);
    QFile file(filePath);
    file.remove();
}

TEST_F(RepoTest, remoteDelete)
{
    auto ret = ostreeRepo->remoteDelete(repoPath);
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
}

TEST_F(RepoTest, checkoutAll)
{
    auto ret = ostreeRepo->checkoutAll(package::Ref("", "linglong", "test", "latest", "x86_64", ""),
                                       commitDir,
                                       QDir::currentPath());
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    QString filePath = QDir::currentPath() + "/" + commitFile;
    auto result = executeTestScript("test_checkout", QStringList{ filePath });
    EXPECT_EQ(result, 0);
    QFile file(filePath);
    file.remove();
}

TEST_F(RepoTest, localLatestRef)
{
    auto ret = ostreeRepo->localLatestRef(package::Ref("test"));
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    qInfo() << (*ret).toSpecString();
    EXPECT_EQ(ret.has_value(), true);
}

TEST_F(RepoTest, checkOutAppData)
{
    auto ret = ostreeRepo->checkOutAppData(repoPath,
                                           NULL,
                                           package::Ref("test").toOSTreeRefLocalString(),
                                           QDir::currentPath());
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    QString filePath = QDir::currentPath() + "/" + testCommitPath;
    auto result = executeTestScript("test_checkout", QStringList{ filePath });
    EXPECT_EQ(result, 0);
    QDir dir(QDir::currentPath() + "/" + commitDir);
    dir.removeRecursively();
}

TEST_F(RepoTest, compressOstreeData)
{
    auto ret = ostreeRepo->compressOstreeData(package::Ref("test"));
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    const auto savePath =
      QStringList{ util::getUserFile(".linglong/builder"), "test" }.join(QDir::separator());
    QFile file(savePath + "/" + testCommitPath);
    EXPECT_EQ(file.exists(), true);
    QFile tgzFile(util::getUserFile(".linglong/builder/") + "test.tgz");
    EXPECT_EQ(tgzFile.exists(), true);
    tgzFile.remove();
    file.remove();
}

TEST_F(RepoTest, repoDeleteDatabyRef)
{
    auto ret =
      ostreeRepo->repoDeleteDatabyRef(repoPath, package::Ref("test").toOSTreeRefLocalString());
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
}

TEST_F(RemoteRepoTest, repoPullbyCmd)
{
    auto ok = qputenv("LINGLONG_ROOT", QDir::currentPath().toLocal8Bit());
    EXPECT_EQ(ok, true);
    auto ret =
      ostreeRepo->repoPullbyCmd(QStringList{ QDir::currentPath(), "repo" }.join(QDir::separator()),
                                remoteRepoName,
                                "org.deepin.music/6.0.1.54/x86_64");
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
}

TEST_F(RemoteRepoTest, push)
{
    GTEST_SKIP();
}

TEST_F(RemoteRepoTest, pull)
{
    int index = QDir::currentPath().lastIndexOf('/');
    auto path =
      QStringList{ QDir::currentPath().left(index), "misc", "linglong", "config.yaml" }.join(
        QDir::separator());
    qInfo() << path;
    auto ok = qputenv("LINGLONG_ROOT", path.toLocal8Bit());
    package::Ref ref = package::Ref("", "org.dde.calendar", "5.9.2.4", "x86_64");
    auto ret = ostreeRepo->pull(ref, false);
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
    auto result =
      executeTestScript("test_pull",
                        QStringList{ localRepoPath, remoteRepoName, ref.toLocalRefString() });
    EXPECT_EQ(result, 0);
}

TEST_F(RemoteRepoTest, remoteShowUrl)
{
    auto ret = ostreeRepo->remoteShowUrl(remoteRepoName);
    if (!ret.has_value()) {
        std::cerr << ret.error().message().toStdString();
    }
    EXPECT_EQ(ret.has_value(), true);
}

TEST_F(RepoTest, rootOfLayer)
{
    GTEST_SKIP();
}

TEST_F(RepoTest, latestOfRef)
{
    GTEST_SKIP();
}

TEST_F(RemoteRepoTest, remoteLatestRef)
{
    GTEST_SKIP();
}

TEST_F(RemoteRepoTest, getRemoteRepoList)
{
    GTEST_SKIP();
}

TEST_F(RemoteRepoTest, getRemoteRefs)
{
    GTEST_SKIP();
}

} // namespace
} // namespace linglong::repo::test
