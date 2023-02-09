/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "src/module/runtime/container.h"
#include "src/module/runtime/oci.h"

#include <QFile>
#include <QJsonDocument>
#include <QtDBus>

TEST(Serialize, QtJsonOCI)
{
    linglong::runtime::registerAllOciMetaType();

    QFile jsonFile("../../test/data/demo/config-mini.json");
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    QVariant variant = json.toVariant();
    auto r = variant.value<Runtime *>();

    EXPECT_EQ(r->root->parent(), r);
    EXPECT_EQ(r->mounts.at(1)->parent(), r);
    EXPECT_EQ(r->ociVersion, "1.0.1");
    EXPECT_EQ(r->process->args[0], "/bin/bash");
    EXPECT_EQ(r->process->env[1], "TERM=xterm");
    EXPECT_EQ(r->mounts.at(1)->options.at(1), "strictatime");
    EXPECT_EQ(r->linux->namespaces.size(), 4);
    EXPECT_EQ(r->root->readonly, false);
    EXPECT_EQ(r->root->path, "/run/user/1000/linglong/ab24ae64edff4ddfa8e6922eb29e2baf");

    EXPECT_EQ(r->linux->uidMappings.size(), 2);
    EXPECT_EQ(r->linux->uidMappings.at(1)->hostId, 1000);
    EXPECT_EQ(r->linux->uidMappings.at(1)->containerId, 1000);
    EXPECT_EQ(r->linux->uidMappings.at(1)->size, 1);

    EXPECT_EQ(r->linux->gidMappings.size(), 2);
    EXPECT_EQ(r->linux->gidMappings.at(0)->hostId, 65534);
    EXPECT_EQ(r->linux->gidMappings.at(0)->containerId, 0);
    EXPECT_EQ(r->linux->gidMappings.at(0)->size, 1);

    r->deleteLater();
}

TEST(Serialize, QByteArray)
{
    QByteArray data = "23282";
    EXPECT_EQ(QString::fromLatin1(data).toLongLong(), 23282);
}
