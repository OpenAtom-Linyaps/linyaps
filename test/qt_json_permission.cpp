/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Heysion <heysion@deepin.com>
 *
 * Maintainer: Heysion <heysion@deepin.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>

#include "src/module/package/package.h"
#include "src/module/package/info.h"
#include "src/module/util/xdg.h"

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