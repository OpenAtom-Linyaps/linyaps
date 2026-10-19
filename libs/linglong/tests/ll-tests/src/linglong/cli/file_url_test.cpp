// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/cli/cli.h"
#include "linglong/utils/env.h"

#include <QTemporaryDir>
#include <QUrl>

#include <filesystem>
#include <fstream>
#include <memory>

namespace linglong::cli {

class CliFileUrlTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp.isValid());
        root = std::filesystem::canonical(temp.path().toStdString());
        home = root / "home";
        std::filesystem::create_directory(home);
        homeGuard = std::make_unique<utils::EnvironmentVariableGuard>("HOME", home.string());
    }

    static std::string mapUrl(const std::string &url) { return Cli::mappingUrl(url); }

    static std::string mapFile(const std::filesystem::path &path) { return Cli::mappingFile(path); }

    static std::string fileUrl(const std::filesystem::path &path)
    {
        return QUrl::fromLocalFile(QString::fromStdString(path.string()))
          .toEncoded(QUrl::FullyEncoded)
          .toStdString();
    }

    QTemporaryDir temp;
    std::filesystem::path root;
    std::filesystem::path home;
    std::unique_ptr<utils::EnvironmentVariableGuard> homeGuard;
};

TEST_F(CliFileUrlTest, LocalhostAuthorityMapsToHostRoot)
{
    auto url = QUrl::fromLocalFile(QString::fromStdString((root / "outside").string()));
    url.setHost("localhost");
    EXPECT_EQ(mapUrl(url.toEncoded().toStdString()), fileUrl(mapFile(root / "outside")));
}

TEST_F(CliFileUrlTest, DecodesSymlinkNamesBeforeMappingAndEncodesOutput)
{
    const auto target = root / "outside space # percent% 中文";
    std::ofstream(target) << "content";
    for (const auto &name : { "link space", "link#hash", "link%percent", "中文链接" }) {
        const auto link = home / name;
        std::filesystem::create_symlink(target, link);
        const auto actual = mapUrl(fileUrl(link));
        EXPECT_EQ(actual, fileUrl(mapFile(link))) << name;
        EXPECT_EQ(QUrl::fromEncoded(QByteArray::fromStdString(actual)).toLocalFile().toStdString(),
                  mapFile(link));
    }
}

TEST_F(CliFileUrlTest, EncodedHomeFilesKeepLocalPath)
{
    const auto path = home / "space # percent% 中文";
    std::ofstream(path) << "content";
    EXPECT_EQ(mapUrl(fileUrl(path)), fileUrl(path));
}

TEST_F(CliFileUrlTest, PreservesRemoteAuthoritiesAndNonFileSchemes)
{
    for (const auto &url : { "file://remote.example/tmp/file%20name",
                             "https://example.com/a%20b",
                             "mailto:user@example.com",
                             "file:///invalid%ZZ" }) {
        EXPECT_EQ(mapUrl(url), url);
    }
}

TEST_F(CliFileUrlTest, AcceptsFileSchemeWithoutDoubleSlashAndPreservesNativePathHandling)
{
    const auto path = root / "outside";
    EXPECT_EQ(mapUrl("file:" + path.string()), fileUrl(mapFile(path)));
    EXPECT_EQ(mapUrl(path.string()), mapFile(path));
}

TEST_F(CliFileUrlTest, PreservesQueryAndFragmentWhileMappingPath)
{
    auto url = QUrl::fromLocalFile(QString::fromStdString((root / "outside").string()));
    url.setQuery("key=value");
    url.setFragment("section");
    const auto actual =
      QUrl::fromEncoded(QByteArray::fromStdString(mapUrl(url.toEncoded().toStdString())));
    EXPECT_EQ(actual.query(), url.query());
    EXPECT_EQ(actual.fragment(), url.fragment());
    EXPECT_EQ(actual.toLocalFile().toStdString(), mapFile(root / "outside"));
}

} // namespace linglong::cli
