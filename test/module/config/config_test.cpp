/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QFile>
#include <QStandardPaths>

TEST(Config, ConfigDir)
{
    if (!qEnvironmentVariableIsSet("LINGLONG_TEST_ALL")) {
        return;
    }

    QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    configDirs.push_back("/etc");

    EXPECT_EQ(configDirs.length(), 3);
    EXPECT_EQ(configDirs[0].endsWith("/.config"), true);
    EXPECT_EQ(configDirs[1], "/etc/xdg");
    EXPECT_EQ(configDirs[2], "/etc");
}
