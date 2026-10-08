// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/package/layer_file.h"

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QIODevice>
#include <QSharedPointer>

#include <filesystem>
#include <fstream>

#include <fcntl.h>

using namespace linglong::package;

namespace {

constexpr auto VALID_META_JSON = R"({"info":{"id":"com.example.App"},"version":"1"})";

// Builds a layer archive on disk: magic number + meta size + meta JSON bytes.
void writeLayerArchive(const std::filesystem::path &path, const QByteArray &metaJson)
{
    QFile file(QString::fromStdString(path.string()));
    file.open(QIODevice::WriteOnly);
    file.write(magicNumber());

    QByteArray sizeBytes;
    QDataStream sizeStream(&sizeBytes, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::LittleEndian);
    sizeStream << quint32(metaJson.size());

    file.write(sizeBytes);
    file.write(metaJson);
    file.close();
}

} // namespace

class LayerFileTest : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(LayerFileTest, MagicNumberLayout)
{
    const auto &number = magicNumber();
    EXPECT_EQ(number.size(), 40);
    EXPECT_TRUE(number.startsWith("<<< deepin linglong layer archive >>>"));
}

TEST_F(LayerFileTest, NewFromValidPathAndReadMetaInfo)
{
    auto layerPath = tempDir.path() / "app.layer";
    const auto metaJson = QByteArray(VALID_META_JSON);
    writeLayerArchive(layerPath, metaJson);

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    // binaryDataOffset reads the meta size first (uncached path)...
    auto offset = (*layer)->binaryDataOffset();
    ASSERT_TRUE(offset.has_value());
    EXPECT_EQ(*offset, magicNumber().size() + sizeof(quint32) + metaJson.size());

    // ...then metaInfo() reuses the cached length from the current position.
    auto info = (*layer)->metaInfo();
    ASSERT_TRUE(info.has_value()) << info.error().message();
    EXPECT_EQ(info->version, "1");
    EXPECT_EQ(info->info.at("id").get<std::string>(), "com.example.App");
}

TEST_F(LayerFileTest, NewFromFileDescriptor)
{
    auto layerPath = tempDir.path() / "fd.layer";
    writeLayerArchive(layerPath, QByteArray(VALID_META_JSON));

    const int fd = ::open(layerPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    auto layer = LayerFile::New(fd);
    ASSERT_TRUE(layer.has_value()) << layer.error().message();

    auto info = (*layer)->metaInfo();
    ASSERT_TRUE(info.has_value());
}

TEST_F(LayerFileTest, NewRejectsInvalidMagicNumber)
{
    auto layerPath = tempDir.path() / "invalid_magic.layer";
    QFile file(QString::fromStdString(layerPath.string()));
    file.open(QIODevice::WriteOnly);
    file.write(QByteArray(40, 'x'));
    file.close();

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_FALSE(layer.has_value());
}

TEST_F(LayerFileTest, NewFromMissingPathFails)
{
    auto layerPath = tempDir.path() / "does_not_exist.layer";
    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_FALSE(layer.has_value());
    EXPECT_NE(layer.error().message(), "");
}

TEST_F(LayerFileTest, MetaInfoRejectsInvalidJson)
{
    auto layerPath = tempDir.path() / "bad_json.layer";
    writeLayerArchive(layerPath, QByteArray("this is not json"));

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_TRUE(layer.has_value());

    auto info = (*layer)->metaInfo();
    ASSERT_FALSE(info.has_value());
}

TEST_F(LayerFileTest, MetaInfoRejectsTruncatedPayload)
{
    auto layerPath = tempDir.path() / "truncated.layer";
    const auto metaJson = QByteArray(VALID_META_JSON);
    writeLayerArchive(layerPath, metaJson.left(metaJson.size() / 2));

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_TRUE(layer.has_value());

    auto info = (*layer)->metaInfo();
    ASSERT_FALSE(info.has_value());
}

TEST_F(LayerFileTest, MetaInfoLengthFailsOnTruncatedHeader)
{
    auto layerPath = tempDir.path() / "short_header.layer";
    QFile file(QString::fromStdString(layerPath.string()));
    file.open(QIODevice::WriteOnly);
    file.write(magicNumber());
    file.close();

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_TRUE(layer.has_value());

    auto info = (*layer)->metaInfo();
    ASSERT_FALSE(info.has_value());
}

// LayerFile instances are always opened from a raw file descriptor, so the
// underlying QFile has no file name and saveTo() reports an error.
TEST_F(LayerFileTest, SaveToFailsForDescriptorBackedFile)
{
    auto layerPath = tempDir.path() / "source.layer";
    writeLayerArchive(layerPath, QByteArray(VALID_META_JSON));

    auto layer = LayerFile::New(QString::fromStdString(layerPath.string()));
    ASSERT_TRUE(layer.has_value());

    auto dest = tempDir.path() / "copied.layer";
    auto result = (*layer)->saveTo(QString::fromStdString(dest.string()));
    ASSERT_FALSE(result.has_value());
}
