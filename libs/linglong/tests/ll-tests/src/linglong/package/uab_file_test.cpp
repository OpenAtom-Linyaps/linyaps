// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/uab_file.h"
#include "linglong/package/uab_packager.h"
#include "linglong/utils/command/cmd.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <filesystem>
#include <fstream>
#include <string>

#include <fcntl.h>

using namespace linglong;

namespace linglong::package {

class MockUabFile : public package::UABFile
{
public:
    MOCK_METHOD(utils::error::Result<void>, mkdirDir, (const std::string &path), (noexcept));
    MOCK_METHOD(bool, isFileReadable, (const std::string &path), (const));
    MOCK_METHOD(bool, checkCommandExists, (const std::string &command), (const));

    explicit MockUabFile(const std::string &path)
        : UABFile()
    {
        auto fd = ::open(path.c_str(), O_RDONLY);
        EXPECT_NE(fd, -1) << "Failed to open uab file" << strerror(errno);
        this->open(fd, QIODevice::ReadOnly, FileHandleFlag::AutoCloseHandle);
        elf_version(EV_CURRENT);
        auto *elf = elf_begin(fd, ELF_C_READ, nullptr);
        this->UABFile::e = elf;
        // 使用 lambda 表达式调用基类方法
        ON_CALL(*this, checkCommandExists(testing::_))
          .WillByDefault(testing::Invoke([this](const std::string &command) {
              return this->UABFile::checkCommandExists(command);
          }));
        ON_CALL(*this, isFileReadable(testing::_))
          .WillByDefault(testing::Invoke([this](const std::string &path) {
              return this->UABFile::isFileReadable(path);
          }));
        ON_CALL(*this, mkdirDir(testing::_))
          .WillByDefault(
            testing::Invoke([this](const std::string &path) -> utils::error::Result<void> {
                return this->UABFile::mkdirDir(path);
            }));
    }
};

class UabFileTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        char tempPath[] = "/var/tmp/linglong-uab-file-test-XXXXXX";
        testDir = mkdtemp(tempPath);
        ASSERT_FALSE(testDir.empty()) << "Failed to create temporary directory";
        uabFile = testDir / "test.uab";
        std::filesystem::copy_file("/proc/self/exe", uabFile);
        auto uab = elfHelper::create(QString::fromStdString(uabFile).toLocal8Bit());
        ASSERT_TRUE(uab.has_value()) << "Failed to create uab file";
        // 添加bundle section
        std::string bundleFile = testDir / "bundle.erofs";
        {
            std::error_code ec;
            std::filesystem::create_directories(testDir / "bundle/layers/test/binary", ec);
            ASSERT_FALSE(ec) << "Failed to create test directory";
            std::string helloFilePath =
              testDir / "bundle" / "layers" / "test" / "binary" / "info.json";
            std::ofstream tmpFile(helloFilePath);
            tmpFile << "Hello, World!";
            tmpFile.close();
            auto ret = utils::command::Cmd("mkfs.erofs")
                         .exec({ bundleFile.c_str(), (testDir / "bundle").c_str() });
            ASSERT_TRUE(ret.has_value())
              << "Failed to create erofs file" << ret.error().message().toStdString();
            auto ret2 = uab->addNewSection("linglong.bundle", QFileInfo(bundleFile.c_str()));
            ASSERT_TRUE(ret2.has_value())
              << "Failed to add bundle section" << ret2.error().message().toStdString();
        }
        // 新添加 meta section
        {
            api::types::v1::PackageInfoV2 packageInfo;
            packageInfo.name = "hello";
            packageInfo.version = "1";
            packageInfo.description = "hello world";
            packageInfo.id = "hello";
            packageInfo.version = "1";
            api::types::v1::UabMetaInfo meta;
            meta.version = api::types::v1::Version::The1;
            meta.uuid = "b2f33c7b-615c-4d7d-9181-e1a22010a749";
            meta.onlyApp = true;
            meta.sections.bundle = "linglong.bundle";
            meta.layers.push_back(api::types::v1::UabLayer{ packageInfo, false });
            // 计算bundle哈希值
            QFile bundle{ bundleFile.c_str() };
            if (!bundle.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
                ASSERT_TRUE(false) << "Failed to open bundle file";
            }
            QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
            if (!cryptor.addData(&bundle)) {
                ASSERT_TRUE(false) << "Failed to add data to cryptor";
            }
            bundle.close();
            meta.digest = cryptor.result().toHex().toStdString();

            std::ofstream jsonFile(testDir / "info.json");
            jsonFile << nlohmann::json(meta).dump();
            jsonFile.close();
            auto ret = uab->addNewSection("linglong.meta",
                                          QFileInfo((testDir / "info.json").string().c_str()));
            ASSERT_TRUE(ret.has_value()) << "Failed to add meta section";
        }
        // 再添加sign section
        {
            auto signDir = testDir / "sign";
            std::error_code ec;
            std::filesystem::create_directories(signDir, ec);
            ASSERT_FALSE(ec) << "Failed to create sign directory";
            std::string helloFilePath = signDir / "hello";
            std::ofstream tmpFile(helloFilePath);
            tmpFile << "Hello, World!";
            tmpFile.close();
            auto ret = utils::command::Cmd("tar").exec(
              { "-cvf", (testDir / "sign.tar").c_str(), "-C", signDir.c_str(), "." });
            ASSERT_TRUE(ret.has_value()) << "Failed to create tar file";
            auto ret2 = uab->addNewSection("linglong.bundle.sign",
                                           QFileInfo((testDir / "sign.tar").string().c_str()));
            ASSERT_TRUE(ret2.has_value()) << "Failed to add sign section";
        }
    }

    static void TearDownTestSuite()
    {
        std::error_code ec;
        std::filesystem::remove_all(testDir, ec);
        ASSERT_FALSE(ec) << "Failed to remove test dir";
    }

    void SetUp() override { }

    void TearDown() override { }

    static std::string uabFile;
    static std::filesystem::path testDir;
};

std::string UabFileTest::uabFile;
std::filesystem::path UabFileTest::testDir;

TEST_F(UabFileTest, UnpackFuseOffset)
{
    // 打开UAB文件
    int fd = open(uabFile.c_str(), O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open uab file" << strerror(errno);

    // 初始化UABFile对象
    auto uab = linglong::package::UABFile::loadFromFile(fd);
    ASSERT_TRUE(uab.has_value()) << "Failed to load uab file";
    auto uabValue = *uab;
    auto unpackRet = uabValue->unpack();
    ASSERT_TRUE(unpackRet.has_value())
      << "Failed to unpack uab file" << unpackRet.error().message().toStdString();

    ASSERT_TRUE(std::filesystem::exists(*unpackRet / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << *unpackRet / "info.json";
}

TEST_F(UabFileTest, UnpackFuse)
{
    auto uab = MockUabFile(uabFile);
    EXPECT_CALL(uab, mkdirDir(testing::_))
      .WillOnce([]() {
          LINGLONG_TRACE("test");
          return LINGLONG_ERR("Cannot create directory in /var/tmp");
      })
      .WillOnce([](const std::string &path) -> utils::error::Result<void> {
          LINGLONG_TRACE("test");
          std::filesystem::create_directories(path);
          return LINGLONG_OK;
      });
    EXPECT_CALL(uab, isFileReadable(testing::_)).WillOnce(testing::Return(false));
    auto unpackRet = uab.unpack();
    ASSERT_TRUE(unpackRet.has_value()) << "Failed to unpack uab file";

    ASSERT_TRUE(std::filesystem::exists(*unpackRet / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << *unpackRet / "info.json";
}

TEST_F(UabFileTest, UnpackFsck)
{
    auto uab = MockUabFile(uabFile);
    EXPECT_CALL(uab, checkCommandExists(testing::_))
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));
    auto unpackRet = uab.unpack();
    ASSERT_TRUE(unpackRet.has_value()) << "Failed to unpack uab file";

    ASSERT_TRUE(std::filesystem::exists(*unpackRet / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << *unpackRet / "info.json";
}

TEST_F(UabFileTest, Verify)
{
    auto uab = MockUabFile(uabFile);
    auto verifyRet = uab.verify();
    ASSERT_TRUE(verifyRet.has_value()) << "Failed to verify uab file";
    ASSERT_TRUE(*verifyRet) << "Verify failed";
}

TEST_F(UabFileTest, ExtractSignData)
{
    auto uab = MockUabFile(uabFile);
    auto ret = uab.unpack();
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack uab file "
                                 << ret.error().message().toStdString();
    auto extractSignDataRet = uab.extractSignData();
    ASSERT_TRUE(extractSignDataRet.has_value())
      << "Failed to extract sign data " << extractSignDataRet.error().message().toStdString();
    auto signDataDir = *extractSignDataRet / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    ASSERT_TRUE(std::filesystem::exists(signDataDir / "hello"))
      << "Failed to extract sign data " << signDataDir / "hello";
    std::ifstream helloFile(signDataDir / "hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
    std::error_code ec;
    std::filesystem::remove_all(*extractSignDataRet, ec);
    ASSERT_FALSE(ec) << "Failed to remove extractSignDataRet" << ec.message();
}

} // namespace linglong::package