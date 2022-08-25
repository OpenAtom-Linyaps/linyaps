/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "serialize_test.h"

#include <QDebug>
#include <mutex>

void serializeRegister()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        qJsonRegister<linglong::repo::Pair>();
        qJsonRegister<linglong::repo::UploadTaskRequest>();
    });
}

TEST(Serialize, Base)
{
    serializeRegister();

    linglong::repo::UploadTaskRequest req;
    req.objects = QStringList {"test1", "test2"};
    req.refs["refs1"] = QPointer<linglong::repo::Pair>(new linglong::repo::Pair);
    req.refs["refs1"]->first = "f1";
    req.refs["refs1"]->second = "s1";

    auto reqJsonBytes = R"MLS(
{
    "refs": {
        "refs_1":{
            "first":"f2",
            "second":"s2"
        }
    },
    "objects":["obj_name_1"]
}
)MLS";

    QScopedPointer<linglong::repo::UploadTaskRequest> newReq(
        linglong::util::loadJsonBytes<linglong::repo::UploadTaskRequest>(reqJsonBytes));

    EXPECT_EQ(newReq->objects.length(), 1);
    EXPECT_EQ(newReq->refs.keys().length(), 1);
    EXPECT_EQ(newReq->refs["refs_1"]->parent(), newReq.data());
}

TEST(Serialize, List)
{
    serializeRegister();

    auto getList = []() -> linglong::repo::PairList {
        auto reqJsonBytes = R"MLS(
{
    "refList": [
       {
           "first":"f1",
           "second":"s2"
       },
       {
           "first":"f2",
           "second":"s2"
       }
    ]
}
)MLS";

        QScopedPointer<linglong::repo::UploadTaskRequest> newReq(
            linglong::util::loadJsonBytes<linglong::repo::UploadTaskRequest>(reqJsonBytes));

        EXPECT_EQ(newReq->refList.length(), 2);
        return newReq->refList;
    };

    auto list = getList();

    EXPECT_EQ(list.length(), 2);
    EXPECT_NE(list[0].data(), nullptr);
}

TEST(Serialize, Destructor)
{
    class Child
    {
    public:
        Child() { qCritical() << "child constructor" << this; }
        Child(const Child &c) { qCritical() << "child copy constructor" << this << "from" << &c; }
        ~Child() { qCritical() << "child destructor" << this; }
    };
    class Parent
    {
    public:
        Parent() { qCritical() << "parent constructor" << this; }
        ~Parent() { qCritical() << "parent destructor" << this; }
        Child child;
    };

    auto getChild = []() -> Child {
        QScopedPointer<Parent> parent(new Parent);
        return parent->child;
    };

    auto child = getChild();
}
