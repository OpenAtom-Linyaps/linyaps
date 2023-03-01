/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "serialize_test.h"

#include <gtest/gtest.h>

#include <QDebug>

#include <mutex>

void serializeRegister()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        qJsonRegister<linglong::repo::TestPair>();
        qJsonRegister<linglong::repo::TestUploadTaskRequest>();
    });
}

TEST(Serialize, Base)
{
    serializeRegister();

    linglong::repo::TestUploadTaskRequest req;
    req.objects = QStringList{ "test1", "test2" };
    req.refs["refs1"] = QPointer<linglong::repo::TestPair>(new linglong::repo::TestPair);
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

    QScopedPointer<linglong::repo::TestUploadTaskRequest> newReq(
            linglong::util::loadJsonBytes<linglong::repo::TestUploadTaskRequest>(reqJsonBytes));

    EXPECT_EQ(newReq->objects.length(), 1);
    EXPECT_EQ(newReq->refs.keys().length(), 1);
    EXPECT_EQ(newReq->refs["refs_1"]->parent(), newReq.data());
}

TEST(Serialize, List)
{
    serializeRegister();

    auto getList = []() -> linglong::repo::TestPairList {
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

        QScopedPointer<linglong::repo::TestUploadTaskRequest> newReq(
                linglong::util::loadJsonBytes<linglong::repo::TestUploadTaskRequest>(reqJsonBytes));

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
