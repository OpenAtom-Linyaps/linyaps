/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/util/qserializer/yaml.h"
#include "object.h"

#include <qdbusargument.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <yaml-cpp/yaml.h>

TEST(QSerializer, Base)
{
    auto sourceJson = QString(R"({
                "some_prop": "Some Prop",
                "some_list": [
                        { "some_prop": "sub1" },
                        { "some_prop": "sub2" }
                ],
                "some_dict": {
                        "key1": "v1",
                        "key2": "v2",
                        "key3": "v3"
                }
        })")
                        .toUtf8();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(sourceJson, &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);

    QVariant v = doc.object().toVariantMap();
    auto obj = v.value<QSharedPointer<linglong::test::TestObject>>();

    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->someProp, "Some Prop");
    ASSERT_EQ(obj->someList.length(), 2);
    ASSERT_EQ(obj->someList[0]->someProp, "sub1");
    ASSERT_EQ(obj->someList[1]->someProp, "sub2");
    ASSERT_EQ(obj->someDict.size(), 3);
    ASSERT_EQ(obj->someDict["key1"], "v1");
    ASSERT_EQ(obj->someDict["key2"], "v2");
    ASSERT_EQ(obj->someDict["key3"], "v3");

    auto vmap = v.fromValue(obj).toMap();
    ASSERT_EQ(vmap.value("some_prop").toString(), "Some Prop");
    ASSERT_EQ(vmap.value("some_list").toList().size(), 2);
    ASSERT_EQ(vmap.value("some_list").toList()[0].toMap().value("some_prop").toString(), "sub1");
    ASSERT_EQ(vmap.value("some_list").toList()[1].toMap().value("some_prop").toString(), "sub2");
    ASSERT_EQ(vmap.value("some_dict").toMap().size(), 3);
    ASSERT_EQ(vmap.value("some_dict").toMap()["key1"], "v1");
    ASSERT_EQ(vmap.value("some_dict").toMap()["key2"], "v2");
    ASSERT_EQ(vmap.value("some_dict").toMap()["key3"], "v3");

    auto jsonObject = QJsonObject::fromVariantMap(vmap);

    auto expectedJson = QString(R"({
        "some_prop": "Some Prop",
        "some_list": [
            {
                "some_prop": "sub1",
                "some_list": [],
                "some_dict": {}
            },
            {
                "some_prop": "sub2",
                "some_list": [],
                "some_dict": {}

            }
        ],
        "some_dict": {
            "key1": "v1",
            "key2": "v2",
            "key3": "v3"
        }
    })")
                          .toUtf8();
    doc = QJsonDocument::fromJson(expectedJson, &err);
    ASSERT_EQ(err.error, QJsonParseError::NoError);

    ASSERT_EQ(QJsonValue(jsonObject), QJsonValue(doc.object()));

    v = jsonObject.toVariantMap();

    obj = v.value<QSharedPointer<linglong::test::TestObject>>();
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->someProp, "Some Prop");
    ASSERT_EQ(obj->someList.length(), 2);
    ASSERT_EQ(obj->someList[0]->someProp, "sub1");
    ASSERT_EQ(obj->someList[1]->someProp, "sub2");
    ASSERT_EQ(obj->someDict.size(), 3);
    ASSERT_EQ(obj->someDict["key1"], "v1");
    ASSERT_EQ(obj->someDict["key2"], "v2");
    ASSERT_EQ(obj->someDict["key3"], "v3");

    auto node = v.value<YAML::Node>();
    YAML::Emitter emitter;
    emitter << node;
    qDebug() << emitter.c_str();
    // TODO(black_desk): YAML test unfinished.
}
