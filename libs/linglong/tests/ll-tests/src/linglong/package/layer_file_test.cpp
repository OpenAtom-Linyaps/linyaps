// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/package/layer_file.h"

#include <QDataStream>
#include <QFile>

#include <filesystem>

namespace {

void writeLayerPrefix(const std::filesystem::path &path,
                      quint32 metadataLength,
                      const QByteArray &metadata = {})
{
    QFile file(QString::fromStdString(path.string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(linglong::package::magicNumber()),
              linglong::package::magicNumber().size());

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << metadataLength;
    ASSERT_EQ(file.write(metadata), metadata.size());
}

} // namespace

TEST(LayerFileTest, RejectsMetadataLengthAboveLimit)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const auto path = tempDir.path() / "oversized.layer";
    writeLayerPrefix(path, 16U * 1024U * 1024U + 1U);

    auto layer = linglong::package::LayerFile::New(QString::fromStdString(path.string()));
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    auto metadata = (*layer)->metaInfo();
    ASSERT_FALSE(metadata.has_value());
    EXPECT_NE(metadata.error().message().find("exceeds limit"), std::string::npos);
}

TEST(LayerFileTest, RejectsTruncatedMetadataBeforeParsing)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const auto path = tempDir.path() / "truncated.layer";
    writeLayerPrefix(path, 32, "{}");

    auto layer = linglong::package::LayerFile::New(QString::fromStdString(path.string()));
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    auto metadata = (*layer)->metaInfo();
    ASSERT_FALSE(metadata.has_value());
    EXPECT_NE(metadata.error().message().find("truncated"), std::string::npos);
}

TEST(LayerFileTest, RejectsEmptyMetadata)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const auto path = tempDir.path() / "empty.layer";
    writeLayerPrefix(path, 0);

    auto layer = linglong::package::LayerFile::New(QString::fromStdString(path.string()));
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    auto metadata = (*layer)->metaInfo();
    ASSERT_FALSE(metadata.has_value());
    EXPECT_NE(metadata.error().message().find("empty"), std::string::npos);
}

TEST(LayerFileTest, ReadsMetadataWithinLimit)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const auto path = tempDir.path() / "valid.layer";
    const auto encodedMetadata = QByteArray{ "{\"info\":{},\"version\":\"1\"}" };
    writeLayerPrefix(path, static_cast<quint32>(encodedMetadata.size()), encodedMetadata);

    auto layer = linglong::package::LayerFile::New(QString::fromStdString(path.string()));
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    auto metadata = (*layer)->metaInfo();
    ASSERT_TRUE(metadata.has_value()) << metadata.error().message();
    EXPECT_EQ(metadata->version, "1");
}
