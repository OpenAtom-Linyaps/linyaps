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
#include <QtDBus>

#include "src/module/runtime/container.h"
#include "src/module/runtime/oci.h"

TEST(Config, ConfigDir)
{
    QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    configDirs.push_back("/etc");

    EXPECT_EQ(configDirs.length(), 3);
    EXPECT_EQ(configDirs[0].endsWith("/.config"), true);
    EXPECT_EQ(configDirs[1], "/etc/xdg");
    EXPECT_EQ(configDirs[2], "/etc");
}
