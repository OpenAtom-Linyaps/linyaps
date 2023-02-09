/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "src/module/package/info.h"
#include "src/module/package/package.h"
#include "src/module/util/xdg.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>

TEST(PermissionTest, LoadJson)
{
    QStringList userTypeList = linglong::util::getXdgUserDir();

    EXPECT_NE(userTypeList.size(), 0);

    linglong::package::registerAllMetaType();

    QFile jsonFile("../../test/data/demo/info.json");
    jsonFile.open(QIODevice::ReadOnly);

    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    auto variant = json.toVariant();

    EXPECT_NE(variant, "");

    auto info = variant.value<linglong::package::Info *>();

    EXPECT_NE(info, nullptr);

    auto userStaticMount = info->permissions->filesystem->user;
    EXPECT_NE(userStaticMount, nullptr);

    auto userVariant = toVariant<linglong::package::User>(userStaticMount);

    auto userStaticMap = userVariant.toMap();

    EXPECT_NE(userStaticMap.size(), 0);

    // load r/rw key to map
    QMap<QString, QString> userStaticMountMap;
    for (const auto &it : userStaticMap.keys()) {
        auto itValue = userStaticMap.value(it).toString();
        if (itValue != "" && (itValue == "r" || itValue == "rw")) {
            userStaticMountMap.insert(it, itValue);
        }
    }

    // foreach show r/rw key
    for (const auto &key : userStaticMountMap.keys()) {
        qInfo() << key;
        qInfo() << "userStaticMountMap.value(key):" << userStaticMountMap.value(key);
        if (userTypeList.indexOf(key) != -1
            && (userStaticMountMap.value(key) == "r " || userStaticMountMap.value(key) == "rw")) {
            EXPECT_TRUE(true);
        }
        EXPECT_NE(userTypeList.indexOf(key), -1);
    }

    info->deleteLater();
}

TEST(PermissionTest, TestPermission)
{
    linglong::package::registerAllMetaType();

    // load json file
    QFile jsonFile("../../test/data/demo/info.json");
    jsonFile.open(QIODevice::ReadOnly);

    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    QVariant variant = json.toVariant();

    EXPECT_NE(variant, "");

    // convert json to info
    auto r = variant.value<linglong::package::Info *>();

    EXPECT_NE(r, nullptr);

    // check app info
    EXPECT_EQ(r->appid, "cn.wps.wps-office");
    EXPECT_EQ(r->version, "11.1.0.10161");
    EXPECT_EQ(r->arch, QStringList({ "amd64" }));
    EXPECT_EQ(r->kind, "");
    EXPECT_EQ(r->name, "wps-office");
    EXPECT_EQ(r->description, "");
    EXPECT_EQ(r->runtime, "");
    EXPECT_EQ(r->base, "");

    // get user permission
    auto permission = r->permissions;

    EXPECT_NE(permission, nullptr);

    // check permission
    EXPECT_EQ(permission->autostart, true);
    EXPECT_EQ(permission->notification, true);
    EXPECT_EQ(permission->trayicon, true);
    EXPECT_EQ(permission->clipboard, true);
    EXPECT_EQ(permission->account, true);
    EXPECT_EQ(permission->bluetooth, true);
    EXPECT_EQ(permission->camera, true);
    EXPECT_EQ(permission->audioRecord, true);
    EXPECT_EQ(permission->installedApps, true);

    // check user permission
    EXPECT_NE(permission->filesystem, nullptr);
    EXPECT_NE(permission->filesystem->user, nullptr);

    // check user permission with template
    EXPECT_EQ(permission->filesystem->user->desktop, "rw");
    EXPECT_EQ(permission->filesystem->user->documents, "r");
    EXPECT_EQ(permission->filesystem->user->downloads, "");
    EXPECT_EQ(permission->filesystem->user->music, "");
    EXPECT_EQ(permission->filesystem->user->pictures, "");
    EXPECT_EQ(permission->filesystem->user->videos, "");
    EXPECT_EQ(permission->filesystem->user->templates, "");
    EXPECT_EQ(permission->filesystem->user->temp, "");
    EXPECT_EQ(permission->filesystem->user->publicShare, "");

    r->deleteLater();
}