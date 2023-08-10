/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/serialize/yaml.h"

#include <gtest/gtest.h>

#include "utils/serialize/TestStruct.h"

#include <QDir>
#include <QFile>

TEST(TestUtilsSerialize, YAMLBasics)
{
    auto dataFile = QFile("utils/serialize/data.yaml");
    dataFile.open(QIODevice::ReadOnly);
    ASSERT_EQ(dataFile.isOpen(), true) << "test data not found";

    auto data = dataFile.readAll();
    ASSERT_NE(data.size(), 0) << "failed to read test data";

    auto [testStruct1, fromErr] =
            linglong::utils::serialize::yaml::deserialize<QSharedPointer<TestStruct>>(data);
    ASSERT_EQ(bool(fromErr), false)
            << "failed to deserialize from test data" << fromErr.message().toStdString();
    ASSERT_NE(testStruct1, nullptr) << "should not get nullptr when there is no error";

    auto checkContent = [](QSharedPointer<TestStruct> testStruct) {
        ASSERT_EQ(testStruct->someInt, 1);
        ASSERT_EQ(testStruct->someString.toStdString(), "a");
        ASSERT_EQ(testStruct->someFloatList.size(), 5);
        ASSERT_EQ(testStruct->someList.size(), 3);
        ASSERT_EQ(testStruct->someList[0]->someString.toStdString(), "");
        ASSERT_EQ(testStruct->someList[1]->someString.toStdString(), "");
        ASSERT_EQ(testStruct->someList[2]->someString.toStdString(), "b");
        ASSERT_EQ(testStruct->someMap.size(), 2);
        ASSERT_NE(testStruct->someMap.find("c"), testStruct->someMap.end());
        ASSERT_EQ(testStruct->someMap["c"]->someString, "d");
        ASSERT_NE(testStruct->someMap.find("e"), testStruct->someMap.end());
        ASSERT_EQ(testStruct->someMap["e"]->someInt, 2);
    };

    checkContent(testStruct1);

    auto [jsonData, toErr] = linglong::utils::serialize::yaml::serialize(testStruct1);
    ASSERT_EQ(bool(toErr), false) << "failed to serialize testStruct" << toErr.code()
                                  << toErr.message().toStdString();
    ASSERT_NE(jsonData.size(), 0) << "should always get some bytes";

    auto [testStruct2, fromErr2] =
            linglong::utils::serialize::yaml::deserialize<QSharedPointer<TestStruct>>(data);
    ASSERT_EQ(bool(fromErr2), false) << "failed to deserialize from test data" << toErr.code()
                                     << fromErr2.message().toStdString();
    ASSERT_NE(testStruct2, nullptr) << "should not get nullptr when there is no error";

    checkContent(testStruct2);
}
