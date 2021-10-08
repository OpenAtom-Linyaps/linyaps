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
#include <QFileInfo>

TEST(Package, Make_ouap1)
{
    Package pkg02;
    QString pkg_name = "org.deepin.calculator-1.2.2-x86_64.uap";
    QString pkg_ouap_name = "org.deepin.calculator-1.2.2-x86_64.ouap";
    QFileInfo fs(pkg_name);

    if (fs.exists()) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }
    EXPECT_EQ(pkg02.InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(pkg02.MakeUap(), true);
    if (QFileInfo::exists(pkg_name) && QFileInfo("/deepin/linglong").isWritable()) {
        EXPECT_EQ(pkg02.MakeOuap(pkg_name), true);
        EXPECT_EQ(fileExists(pkg_ouap_name), true);
    }

    if (fileExists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }

    if (QFileInfo::exists(pkg_name)) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }

    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/uab.json")), false);
    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/")), false);
    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/sdfgssssert.uap")), false);
}
TEST(Package, Make_ouap2)
{
    Package pkg02;
    QString pkg_name = "org.deepin.calculator-1.2.2-x86_64.uap";
    QString pkg_ouap_name = "org.deepin.calculator-1.2.2-x86_64.ouap";
    QString repo_path = "ttest/repo";
    QFileInfo fs(pkg_name);

    if (fs.exists()) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }
    EXPECT_EQ(pkg02.InitUap(QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"), QString("../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64")), true);
    EXPECT_EQ(pkg02.MakeUap(), true);
    EXPECT_EQ(pkg02.MakeOuap(pkg_name, repo_path), true);
    EXPECT_EQ(dirExists(repo_path), true);
    EXPECT_EQ(fileExists(pkg_ouap_name), true);

    if (dirExists(repo_path)) {
        removeDir(repo_path);
        QDir().rmdir(repo_path.split("/").at(0));
    }
    if (fileExists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }
    if (QFileInfo::exists(pkg_name)) {
        QFile::remove(pkg_name);
    }
    if (QFileInfo::exists(pkg_ouap_name)) {
        QFile::remove(pkg_ouap_name);
    }

    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/uab.json")), false);
    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/")), false);
    EXPECT_EQ(pkg02.MakeOuap(QString("../../test/data/demo/sdfgssssert.uap")), false);
}
