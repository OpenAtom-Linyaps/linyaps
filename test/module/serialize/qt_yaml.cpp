/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "qt_yaml.h"

TEST(Serialize, YAML_NS)
{
    qJsonRegister<linglong::test::MountRule>();
    qJsonRegister<linglong::test::Permission>();
    qJsonRegister<linglong::test::App>();

    auto path = "../../test/data/demo/app.yml";
    YAML::Node doc = YAML::LoadFile(path);

    auto app = formYaml<linglong::test::App>(doc);

    EXPECT_EQ(app->permissions->mounts.value(0)->type, "test_type");
}

TEST(Serialize, YAML)
{
    linglong::runtime::registerAllOciMetaType();

    qJsonRegister<TestMount>();
    qJsonRegister<TestPermission>();
    qJsonRegister<TestApp>();

    auto path = "../../test/data/demo/app.yml";
    YAML::Node doc = YAML::LoadFile(path);

    auto app = formYaml<TestApp>(doc);

    EXPECT_EQ(app->root->parent(), app);
    EXPECT_EQ(app->namespaces.value(0)->parent(), app);
    EXPECT_EQ(app->version, "1.12");
    EXPECT_EQ(app->mounts.length(), 3);
    EXPECT_EQ(app->root->readonly, false);
    EXPECT_EQ(app->root->path, "/run/user/1000/linglong/ab24ae64edff4ddfa8e6922eb29e2baf");

    EXPECT_EQ(app->permissions->mounts.value(0)->type, "test_type");

    app->deleteLater();

    TestApp app2;

    app2.root = new Root;
    app2.root->readonly = true;
    app2.version = "2";

    auto doc2 = toYaml<TestApp>(&app2);

    EXPECT_EQ(doc2["version"].as<QString>(), "2");
    EXPECT_EQ(doc2["root"]["readonly"].as<QString>(), "true");
    EXPECT_EQ(doc2["root"]["readonly"].as<bool>(), true);

    app2.root->deleteLater();
}