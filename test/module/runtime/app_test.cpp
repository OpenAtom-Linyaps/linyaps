/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/runtime/app.h"
#include "module/util/qserializer/yaml.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>

TEST(AppTest, StructTest)
{

    // load data from yaml file
    auto app = std::get<0>(linglong::util::fromYAML<QSharedPointer<linglong::runtime::App>>(
            QString("../../test/data/demo/app-test.yaml")));

    EXPECT_NE(app, nullptr);

    // check base info
    EXPECT_EQ(app->version, "1.0");
    EXPECT_EQ(app->package->ref, "org.deepin.calculator/5.7.16/x86_64");
    EXPECT_EQ(app->runtime->ref, "org.deepin.Runtime/20.5.0/x86_64");

    // check permission
    EXPECT_NE(app->permissions, nullptr);
    EXPECT_NE(app->permissions->mounts.size(), 0);
    EXPECT_EQ(app->permissions->mounts[0]->type, "bind");
    EXPECT_EQ(app->permissions->mounts[0]->options, "rw,rbind");
    EXPECT_EQ(app->permissions->mounts[0]->source, "/home/linglong/Desktop");
    EXPECT_EQ(app->permissions->mounts[0]->destination, "/home/linglong/Desktop");
}
