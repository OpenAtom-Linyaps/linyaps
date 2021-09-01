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

#include "module/runtime/container.h"

#include <QJsonDocument>
#include <QFile>

TEST(OCI, QtJson)
{
    ociJsonRegister();

    QFile jsonFile("../../test/data/demo/config-mini.json");
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    QVariant variant = json.toVariant();
    auto r = variant.value<Runtime *>();

    EXPECT_EQ(r->ociVersion, "1.0.1");
    EXPECT_EQ(r->process->args[0], "/bin/bash");
    EXPECT_EQ(r->process->env[1], "TERM=xterm");
    EXPECT_EQ(r->mounts.at(1)->options.at(1), "strictatime");
    EXPECT_EQ(r->linux->namespaces.size(), 4);
    EXPECT_EQ(r->root->readonly, false);
    EXPECT_EQ(r->root->path, "/run/user/1000/linglong/ab24ae64edff4ddfa8e6922eb29e2baf");
}