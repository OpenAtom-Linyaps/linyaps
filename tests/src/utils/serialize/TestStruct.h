/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_UTILS_SERIALIZE_TESTSTRUCT_H
#define LINGLONG_TEST_UTILS_SERIALIZE_TESTSTRUCT_H

#include <qserializer/core.h>

#include <QString>

class TestStruct
{
    Q_GADGET;

public:
    int someInt;
    Q_PROPERTY(int someInt MEMBER someInt);

    QString someString;
    Q_PROPERTY(QString someString MEMBER someString);

    QList<float> someFloatList;
    Q_PROPERTY(QList<float> someFloatList MEMBER someFloatList);

    QList<QSharedPointer<TestStruct>> someList;
    Q_PROPERTY(QList<QSharedPointer<TestStruct>> someList MEMBER someList);

    QMap<QString, QSharedPointer<TestStruct>> someMap;
    Q_PROPERTY(QMap<QString, QSharedPointer<TestStruct>> someMap MEMBER someMap);
};

QSERIALIZER_DECLARE(TestStruct);

#endif
