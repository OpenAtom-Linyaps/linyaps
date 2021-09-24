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

#include "module/util/fs.h"

#include <QString>
#include <QStringList>
#include <QDebug>

using namespace linglong::util;

static bool compareNames(const QString &s1, const QString &s2)
{
    auto v1 = s1.split("/").takeLast();
    auto v2 = s2.split("/").takeLast();
    return v1.toDouble() > v2.toDouble();
}

TEST(mod_util_fs, mod_util_fs)
{
    // listDirFolders
    auto r1 = listDirFolders("/deepin/linglong");
    EXPECT_EQ(r1.first(), "/deepin/linglong/layers");

    auto r2 = listDirFolders("/deepin/linglong", true);
    EXPECT_GT(r2.size(), 1);

    auto r3 = listDirFolders("/deepin/linglong", false);
    EXPECT_EQ(r3.first(), "/deepin/linglong/layers");
    EXPECT_EQ(r3.size(), 1);
}
