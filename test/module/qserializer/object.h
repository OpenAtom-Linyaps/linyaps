/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_QSERIALIZER_OBJECT_H_
#define LINGLONG_TEST_MODULE_QSERIALIZER_OBJECT_H_

#include <QMap>
#include <QObject>

namespace linglong {
namespace test {

class TestObject : public QObject
{
    Q_OBJECT;
    Q_PROPERTY(QString some_prop MEMBER someProp);
    Q_PROPERTY(QList<QSharedPointer<TestObject>> some_list MEMBER someList);
    Q_PROPERTY(QMap<QString, QString> some_dict MEMBER someDict);

public:
    QString someProp;
    QList<QSharedPointer<TestObject>> someList;
    QMap<QString, QString> someDict;
};

namespace QSerializerPrivateNamespaceTestObject {
int init();
static int _ = init();
}; // namespace QSerializerPrivateNamespaceTestObject

} // namespace test
} // namespace linglong

Q_DECLARE_METATYPE(linglong::test::TestObject *);

#endif
