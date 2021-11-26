/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QFile>
#include <module/util/fs.h>

#include "src/module/package/package.h"

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
    QString uap_name = "org.deepin.calculator-1.2.2-x86_64.uap";
    if (fileExists(uap_name)) {
        QFile::remove(uap_name);
    }

    bool rm_pkg_data = true;
    QString uap_data_path = "/deepin/linglong/layers/org.deepin.calculator/1.2.2";

    // init data
    Package *pkg01 = new Package();

    EXPECT_EQ(pkg01->InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(fileExists(pkg01->dataPath), true);
    EXPECT_EQ(dirExists(QFileInfo(pkg01->dataPath).dir().path()), true);
    pkg01->MakeUap();

    EXPECT_EQ(fileExists(uap_name), true);
    delete pkg01;
    pkg01 = nullptr;

    //安装离线包测试
    Package *pkg02 = new Package();

    if (dirExists(uap_data_path)) {
        // check install status
        rm_pkg_data = false;
    }

    if (pkg02->InitUapFromFile(uap_name)) {
        QString yaml_path_link = "/deepin/linglong/layers/org.deepin.calculator/1.2.2/org.deepin.calculator.yaml";

        QString info_path_link = "/deepin/linglong/layers/org.deepin.calculator/1.2.2/x86_64/info";
        // test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64
        EXPECT_EQ(fileExists(yaml_path_link), true);
        EXPECT_EQ(fileExists(info_path_link), true);
        EXPECT_EQ(QFileInfo(yaml_path_link).isSymLink(), true);
        EXPECT_EQ(QFileInfo(info_path_link).isSymLink(), true);
    }
    delete pkg02;
    pkg02 = nullptr;
    if (fileExists(uap_name)) {
        QFile::remove(uap_name);
        if (rm_pkg_data) {
            // remove pkg install data
            removeDir(uap_data_path);
        }
    }
}