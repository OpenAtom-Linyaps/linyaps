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
#include <QFileInfo>

#include "src/module/package/package.h"

TEST(Package, Get_info)
{
    Package pkg01;
    QString pkg_name = "org.deepin.calculator-1.2.2-x86_64.uap";
    QString pkg_info = "org.deepin.calculator-1.2.2-x86_64.uap.info";
    QFileInfo fs(pkg_name);

    if (fs.exists()) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_info)) {
        QFile::remove(pkg_info);
    }
    EXPECT_EQ(pkg01.InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(pkg01.MakeUap(), true);
    EXPECT_EQ(pkg01.GetInfo(fs.fileName()), true);
    if (QFileInfo::exists(pkg_name)) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_info)) {
        QFile::remove(pkg_info);
    }

    EXPECT_EQ(pkg01.GetInfo(QString("../../test/data/demo/uab.json")), false);
    EXPECT_EQ(pkg01.GetInfo(QString("../../test/data/demo/")), false);
    EXPECT_EQ(pkg01.GetInfo(QString("../../test/data/demo/sdfgssssert.uap")), false);
}
