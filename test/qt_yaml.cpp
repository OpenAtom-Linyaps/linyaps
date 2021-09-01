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

#include <fstream>

#include <QFile>

#include "module/util/yaml.h"
#include "module/runtime/app.h"

TEST(LL, YAML)
{
    auto path = "../../test/data/demo/app.yml";
    YAML::Node doc = YAML::LoadFile(path);

    auto app = formYaml<App>(doc);

    EXPECT_EQ(app->version, "1.12");
    EXPECT_EQ(app->mounts.length(), 2);
    EXPECT_EQ(app->root->readonly, false);
    EXPECT_EQ(app->root->path, "/run/user/1000/linglong/ab24ae64edff4ddfa8e6922eb29e2baf");

    App app2;

    app2.root = new Root;
    app2.root->readonly = true;
    app2.version = "2";

    auto doc2 = toYaml<App>(&app2);

    EXPECT_EQ(doc2["version"].as<QString>(), "2");
    EXPECT_EQ(doc2["root"]["readonly"].as<QString>(), "true");
    EXPECT_EQ(doc2["root"]["readonly"].as<bool>(), true);
}