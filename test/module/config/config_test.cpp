/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QFile>
#include <QJsonDocument>
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
