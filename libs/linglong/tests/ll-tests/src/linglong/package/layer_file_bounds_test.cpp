// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/package/layer_file.h"

#include <QDataStream>
#include <QTemporaryFile>

#include <limits>

namespace {

using linglong::package::LayerFile;
using linglong::package::magicNumber;

class LayerFileBoundsTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(file.open()); }

    void writeHeader(quint32 length, const QByteArray &payload = {})
    {
        ASSERT_EQ(file.write(magicNumber()), magicNumber().size());
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << length;
        ASSERT_EQ(stream.status(), QDataStream::Ok);
        ASSERT_EQ(file.write(payload), payload.size());
        ASSERT_TRUE(file.flush());
    }

    QTemporaryFile file;
};

TEST_F(LayerFileBoundsTest, ValidMetadataQueriesAreIndependentOfFilePosition)
{
    const QByteArray metadata = R"({"info":{},"version":"1"})";
    writeHeader(metadata.size(), metadata + "filesystem payload");
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    auto offset = (*layer)->binaryDataOffset();
    ASSERT_TRUE(offset.has_value());
    EXPECT_EQ(*offset, quint64(44 + metadata.size()));
    for (int i = 0; i < 2; ++i) {
        auto info = (*layer)->metaInfo();
        ASSERT_TRUE(info.has_value()) << info.error().message();
        EXPECT_EQ(info->version, "1");
    }
}

TEST_F(LayerFileBoundsTest, RejectsTruncatedLengthField)
{
    ASSERT_EQ(file.write(magicNumber()), magicNumber().size());
    ASSERT_EQ(file.write("xx", 2), 2);
    ASSERT_TRUE(file.flush());
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->metaInfo().has_value());
    EXPECT_FALSE((*layer)->binaryDataOffset().has_value());
}

TEST_F(LayerFileBoundsTest, RejectsMetadataPastEndOfFile)
{
    writeHeader(100, "{}");
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->metaInfo().has_value());
    EXPECT_FALSE((*layer)->binaryDataOffset().has_value());
}

TEST_F(LayerFileBoundsTest, RejectsZeroLength)
{
    writeHeader(0);
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->metaInfo().has_value());
    EXPECT_FALSE((*layer)->binaryDataOffset().has_value());
}

TEST_F(LayerFileBoundsTest, RejectsLengthThatWouldWrapOldOffset)
{
    writeHeader(std::numeric_limits<quint32>::max());
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->binaryDataOffset().has_value());
}

TEST_F(LayerFileBoundsTest, RejectsOversizedMetadataEvenWhenFileIsLargeEnough)
{
    writeHeader((16 * 1024 * 1024) + 1);
    ASSERT_TRUE(file.resize(qint64(44) + (16 * 1024 * 1024) + 1));
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->metaInfo().has_value());
    EXPECT_FALSE((*layer)->binaryDataOffset().has_value());
}

TEST_F(LayerFileBoundsTest, RejectsNulAndTrailingBytes)
{
    QByteArray metadata = R"({"info":{},"version":"1"})";
    metadata.append('\0');
    metadata.append("unparsed bytes");
    writeHeader(metadata.size(), metadata);
    auto layer = LayerFile::New(file.fileName());
    ASSERT_TRUE(layer.has_value());
    EXPECT_FALSE((*layer)->metaInfo().has_value());
}

} // namespace
