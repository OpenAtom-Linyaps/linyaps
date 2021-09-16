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

TEST(Package, Extract)
{
    Package pkg01;
    QString pkg_name = "authpass-1.9.4-x86_64.uap";
    QFileInfo fs(pkg_name);

    if (fs.exists()) {
        QFile::remove(pkg_name);
    }
    EXPECT_EQ(pkg01.InitUap(QString("../../test/data/demo/pkg-demo/pkg01/uap.json"), QString("../../test/data/demo/pkg-demo/pkg01")), true);
    EXPECT_EQ(pkg01.MakeUap(), true);
    EXPECT_EQ(pkg01.Extract(fs.fileName(), QString("test1")), true);
    if (QFileInfo::exists(pkg_name)) {
        QFile::remove(pkg_name);
    }

    EXPECT_EQ(pkg01.Extract(QString("../../test/data/demo/uab.json"), QString("test")), false);
    EXPECT_EQ(pkg01.Extract(QString("../../test/data/demo/"), QString("test")), false);
    EXPECT_EQ(pkg01.Extract(QString("../../test/data/demo/sdfgssssert.uap"), QString("test")), false);
}
