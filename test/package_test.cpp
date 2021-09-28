/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "module/package/package.h"

#include <QJsonDocument>
#include <QFile>
#include <module/util/fs.h>

using namespace linglong::util;

TEST(Package, UaPInit_01)
{
    QFile jsonFile("../../test/data/demo/uab.json");
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    Package pkg01;
    EXPECT_EQ(pkg01.initConfig(QString("../../test/data/demo/pkg-demo/pkg01/uap.json")), true);
    EXPECT_EQ(pkg01.initConfig(QString("../../test/data/demo/")), false);
    EXPECT_EQ(pkg01.initConfig(QString("../../test/data/demo/ohOM5vOCS7ELFIUqsNPLwwoT.json")), false);

    EXPECT_EQ(pkg01.initData(QString("../../test/data/demo/pkg-demo/pkg01")), true);
    EXPECT_EQ(pkg01.initData(QString("../../test/data/demo/uab.json")), false);
    EXPECT_EQ(pkg01.initData(QString("../../test/data/demo/1")), false);
}

TEST(Package, UaPInit_02)
{
    // init data
    Package *pkg01 = new Package();

    EXPECT_EQ(pkg01->InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(fileExists(pkg01->dataPath), true);
    EXPECT_EQ(dirExists(QFileInfo(pkg01->dataPath).dir().path()), true);

    // copy data path
    QString tmp_data = pkg01->dataPath;
    QString tmp_data_path = QFileInfo(pkg01->dataPath).dir().path();

    // release pkg object
    delete pkg01;

    EXPECT_EQ(fileExists(tmp_data), false);
    EXPECT_EQ(dirExists(tmp_data_path), false);
}


TEST(Package, UaPInit_03)
{

    // init data
    Package *pkg01 = new Package();

    EXPECT_EQ(pkg01->InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(fileExists(pkg01->dataPath), true);
    EXPECT_EQ(dirExists(QFileInfo(pkg01->dataPath).dir().path()), true);
    pkg01->MakeUap();
    EXPECT_EQ(true,true);

    // test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64
    delete pkg01;
}