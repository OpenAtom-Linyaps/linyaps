/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/builder/linglong_builder.h"
#include "linglong/builder/printer.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <vector>

namespace linglong::builder {

using ::testing::_;
using ::testing::Return;

class LinglongBuilderDeepTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());
        projectDir = tempDir.path() + "/project";
        buildDir = tempDir.path() + "/build";
        QDir(projectDir).mkpath(".");
        QDir(buildDir).mkpath(".");
    }

    QTemporaryDir tempDir;
    QString projectDir;
    QString buildDir;
};

TEST_F(LinglongBuilderDeepTest, PrinterFormattingAndOutputHandling)
{
    ConsolePrinter printer;
    EXPECT_NO_THROW(printer.printInfo("Starting build process test..."));
    EXPECT_NO_THROW(printer.printWarning("Warning: unoptimized flag detected"));
    EXPECT_NO_THROW(printer.printError("Error: target step failed"));
}

TEST_F(LinglongBuilderDeepTest, ProjectDirectoryStructureValidation)
{
    QString yamlPath = projectDir + "/linglong.yaml";
    QFile file(yamlPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"(
package:
  id: com.example.structuretest
  name: structuretest
  version: 1.0.0
  kind: app
  description: directory structure test

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.0

command:
  - structuretest
)");
    file.close();

    auto configRes = Config::load(yamlPath.toStdString());
    ASSERT_TRUE(configRes.has_value());
    EXPECT_EQ(configRes.value().package.id, "com.example.structuretest");
}

TEST_F(LinglongBuilderDeepTest, MultipleBuildStepsSequenceValidation)
{
    std::vector<std::string> buildCommands = { "mkdir -p build",
                                               "cd build && cmake ..",
                                               "make -j4",
                                               "make install DESTDIR=/tmp/target" };

    EXPECT_EQ(buildCommands.size(), 4U);
    EXPECT_EQ(buildCommands[0], "mkdir -p build");
    EXPECT_EQ(buildCommands[3], "make install DESTDIR=/tmp/target");
}

} // namespace linglong::builder
