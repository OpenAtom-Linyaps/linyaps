// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/gkeyfile_wrapper.h"
#include "linglong/utils/error/error.h"

#include <QTemporaryFile>
#include <QTextStream>

#include <filesystem>
#include <fstream>

class GKeyFileWrapperTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 创建临时文件用于测试
        tempFile = std::make_unique<QTemporaryFile>();
        ASSERT_TRUE(tempFile->open());
        tempFilePath = tempFile->fileName();
        tempFile->close();
    }

    void TearDown() override
    {
        // 清理临时文件
        if (tempFile && tempFile->exists()) {
            tempFile->remove();
        }
    }

    void createTestFile(const QString &content)
    {
        QFile file(tempFilePath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream << content;
        file.close();
    }

    std::unique_ptr<QTemporaryFile> tempFile;
    QString tempFilePath;
};

TEST_F(GKeyFileWrapperTest, NewWithNonExistentFile)
{
    auto result = linglong::common::GKeyFileWrapper::New("/nonexistent/file.ini");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message().find("no such file") != std::string::npos);
}

TEST_F(GKeyFileWrapperTest, NewWithValidFile)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
Exec=/usr/bin/test-app
Icon=test-icon
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    EXPECT_TRUE(result.has_value());
}

TEST_F(GKeyFileWrapperTest, NewWithInvalidFile)
{
    // 创建无效的文件内容
    const QString content = R"([Invalid Section
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    EXPECT_FALSE(result.has_value());
}

TEST_F(GKeyFileWrapperTest, SetAndGetValue)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 设置新值
    wrapper.setValue("Exec", "/usr/bin/new-app", linglong::common::GKeyFileWrapper::DesktopEntry);

    // 获取值
    auto execValue =
      wrapper.getValue<QString>("Exec", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(execValue.has_value());
    EXPECT_EQ(*execValue, "/usr/bin/new-app");

    // 获取已存在的值
    auto nameValue =
      wrapper.getValue<QString>("Name", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(nameValue.has_value());
    EXPECT_EQ(*nameValue, "Test Application");
}

TEST_F(GKeyFileWrapperTest, GetValueWithNonExistentKey)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 获取不存在的键
    auto nonExistentValue =
      wrapper.getValue<QString>("NonExistentKey", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_FALSE(nonExistentValue.has_value());
}

TEST_F(GKeyFileWrapperTest, GetValueWithNonExistentGroup)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 获取不存在的组
    auto nonExistentValue = wrapper.getValue<QString>("Name", "NonExistentGroup");
    EXPECT_FALSE(nonExistentValue.has_value());
}

TEST_F(GKeyFileWrapperTest, GetGroups)
{
    const QString content = R"([Desktop Entry]
Name=Test Application

[D-BUS Service]
Name=com.example.Test

[Service]
ExecStart=/usr/bin/test-service
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    auto groups = wrapper.getGroups();
    EXPECT_EQ(groups.size(), 3);
    EXPECT_TRUE(groups.contains("Desktop Entry"));
    EXPECT_TRUE(groups.contains("D-BUS Service"));
    EXPECT_TRUE(groups.contains("Service"));
}

TEST_F(GKeyFileWrapperTest, GetKeys)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
Exec=/usr/bin/test-app
Icon=test-icon
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    auto keys = wrapper.getkeys(linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(keys.has_value());
    EXPECT_EQ(keys->size(), 3);
    EXPECT_TRUE(keys->contains("Name"));
    EXPECT_TRUE(keys->contains("Exec"));
    EXPECT_TRUE(keys->contains("Icon"));
}

TEST_F(GKeyFileWrapperTest, GetKeysWithNonExistentGroup)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    auto keys = wrapper.getkeys("NonExistentGroup");
    EXPECT_FALSE(keys.has_value());
}

TEST_F(GKeyFileWrapperTest, SaveToFile)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 修改内容
    wrapper.setValue("Exec",
                     "/usr/bin/modified-app",
                     linglong::common::GKeyFileWrapper::DesktopEntry);
    wrapper.setValue("Icon", "modified-icon", linglong::common::GKeyFileWrapper::DesktopEntry);

    // 保存到新文件
    QString newFilePath = tempFilePath + ".new";
    auto saveResult = wrapper.saveToFile(newFilePath);
    EXPECT_TRUE(saveResult.has_value());

    // 验证新文件内容
    auto newWrapper = linglong::common::GKeyFileWrapper::New(newFilePath);
    EXPECT_TRUE(newWrapper.has_value());

    auto execValue =
      newWrapper->getValue<QString>("Exec", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(execValue.has_value());
    EXPECT_EQ(*execValue, "/usr/bin/modified-app");

    auto iconValue =
      newWrapper->getValue<QString>("Icon", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(iconValue.has_value());
    EXPECT_EQ(*iconValue, "modified-icon");

    // 清理新文件
    QFile::remove(newFilePath);
}

TEST_F(GKeyFileWrapperTest, HasKey)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
Exec=/usr/bin/test-app
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 测试存在的键
    auto hasName = wrapper.hasKey("Name", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(hasName.has_value());
    EXPECT_TRUE(*hasName);

    auto hasExec = wrapper.hasKey("Exec", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(hasExec.has_value());
    EXPECT_TRUE(*hasExec);

    // 测试不存在的键
    auto hasNonExistent =
      wrapper.hasKey("NonExistentKey", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(hasNonExistent.has_value());
    EXPECT_FALSE(*hasNonExistent);
}

TEST_F(GKeyFileWrapperTest, HasKeyWithNonExistentGroup)
{
    const QString content = R"([Desktop Entry]
Name=Test Application
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    auto hasKey = wrapper.hasKey("Name", "NonExistentGroup");
    EXPECT_FALSE(hasKey.has_value());
}

TEST_F(GKeyFileWrapperTest, Constants)
{
    // 测试常量定义
    EXPECT_EQ(linglong::common::GKeyFileWrapper::DesktopEntry, "Desktop Entry");
    EXPECT_EQ(linglong::common::GKeyFileWrapper::DBusService, "D-BUS Service");
    EXPECT_EQ(linglong::common::GKeyFileWrapper::SystemdService, "Service");
    EXPECT_EQ(linglong::common::GKeyFileWrapper::ContextMenu, "Menu Entry");
}

TEST_F(GKeyFileWrapperTest, EmptyFile)
{
    // 创建空文件
    createTestFile("");

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    EXPECT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 空文件应该没有组
    auto groups = wrapper.getGroups();
    EXPECT_TRUE(groups.isEmpty());
}

TEST_F(GKeyFileWrapperTest, MultipleOperations)
{
    const QString content = R"([Desktop Entry]
Name=Original Name
)";

    createTestFile(content);

    auto result = linglong::common::GKeyFileWrapper::New(tempFilePath);
    ASSERT_TRUE(result.has_value());

    auto &wrapper = *result;

    // 执行多个操作
    wrapper.setValue("Name", "Updated Name", linglong::common::GKeyFileWrapper::DesktopEntry);
    wrapper.setValue("Exec",
                     "/usr/bin/updated-app",
                     linglong::common::GKeyFileWrapper::DesktopEntry);
    wrapper.setValue("Icon", "updated-icon", linglong::common::GKeyFileWrapper::DesktopEntry);

    // 验证所有更改
    auto nameValue =
      wrapper.getValue<QString>("Name", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(nameValue.has_value());
    EXPECT_EQ(*nameValue, "Updated Name");

    auto execValue =
      wrapper.getValue<QString>("Exec", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(execValue.has_value());
    EXPECT_EQ(*execValue, "/usr/bin/updated-app");

    auto iconValue =
      wrapper.getValue<QString>("Icon", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(iconValue.has_value());
    EXPECT_EQ(*iconValue, "updated-icon");

    // 验证键的存在
    auto hasAllKeys = wrapper.hasKey("Name", linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(hasAllKeys.has_value());
    EXPECT_TRUE(*hasAllKeys);

    // 验证组中的键数量
    auto keys = wrapper.getkeys(linglong::common::GKeyFileWrapper::DesktopEntry);
    EXPECT_TRUE(keys.has_value());
    EXPECT_EQ(keys->size(), 3);
}
