/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "src/module/util/fs.h"

#include <gtest/gtest.h>
#include <QString>
#include <QStringList>
#include <QDebug>

using namespace linglong::util;

TEST(mod_util_fs, mod_util_fs)
{
    // listDirFolders
    bool delete_dir = false;
    auto parent_path = "/tmp/deepin-linglong/layers";
    if (!dirExists(parent_path)) {
        delete_dir = true;
        createDir(QString(parent_path) + "/1.0");
        createDir(QString(parent_path) + "/2.0");
    }

    auto r1 = listDirFolders("/tmp/deepin-linglong");
    EXPECT_NE(r1.empty(), true);
    EXPECT_EQ(r1.first(), parent_path);

    auto r2 = listDirFolders("/tmp/deepin-linglong", true);
    EXPECT_NE(r2.empty(), true);
    EXPECT_GT(r2.size(), 1);

    auto r3 = listDirFolders("/tmp/deepin-linglong", false);
    EXPECT_NE(r3.empty(), true);
    EXPECT_EQ(r3.first(), parent_path);
    EXPECT_EQ(r3.size(), 1);

    if (delete_dir && dirExists(parent_path)) {
        delete_dir = false;
        removeDir(QString("/tmp/deepin-linglong"));
    }
}
