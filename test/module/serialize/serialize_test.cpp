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

TEST(Serialize, Base)
{
    qJsonRegister<linglong::repo::Pair>();
    qJsonRegister<linglong::repo::UploadTaskRequest>();

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

    auto newReq = linglong::util::loadJsonBytes<linglong::repo::UploadTaskRequest>(reqJsonBytes);

    EXPECT_EQ(newReq->objects.length(), 1);
    EXPECT_EQ(newReq->refs.keys().length(), 1);
    EXPECT_EQ(newReq->refs["refs_1"]->parent(), newReq);
}
