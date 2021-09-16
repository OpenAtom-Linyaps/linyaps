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

TEST(Package, UaPInit)
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
