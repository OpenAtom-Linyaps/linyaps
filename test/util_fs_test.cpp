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

TEST(mod_util_fs, mod_util_fs)
{
    // listDirFolders
    bool delete_dir = false;
    auto parent_path = "/tmp/deepin/linglong/layers";
    if (!dirExists(parent_path)) {
        delete_dir = true;
        createDir(QString(parent_path) + "/1.0");
        createDir(QString(parent_path) + "/2.0");
    }

    auto r1 = listDirFolders("/tmp/deepin/linglong");
    EXPECT_NE(r1.empty(), true);
    EXPECT_EQ(r1.first(), parent_path);

    auto r2 = listDirFolders("/tmp/deepin/linglong", true);
    EXPECT_NE(r2.empty(), true);
    EXPECT_GT(r2.size(), 1);

    auto r3 = listDirFolders("/tmp/deepin/linglong", false);
    EXPECT_NE(r3.empty(), true);
    EXPECT_EQ(r3.first(), parent_path);
    EXPECT_EQ(r3.size(), 1);

    if (delete_dir && dirExists(parent_path)) {
        delete_dir = false;
        removeDir(QString(parent_path));
    }
}
