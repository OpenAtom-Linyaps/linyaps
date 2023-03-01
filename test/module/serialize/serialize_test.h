/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_SERIALIZE_SERIALIZE_TEST_H_
#define LINGLONG_TEST_MODULE_SERIALIZE_SERIALIZE_TEST_H_

#include "module/util/serialize/json.h"
#include "module/util/serialize/serialize.h"

namespace linglong {
namespace repo {

class TestPair : public JsonSerialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(TestPair);

public:
    Q_SERIALIZE_PROPERTY(QString, first);
    Q_SERIALIZE_PROPERTY(QString, second);
};

} // namespace repo
} // namespace linglong

Q_SERIALIZE_DECLARE_METATYPE_NM(linglong::repo, TestPair)

namespace linglong {
namespace repo {

class TestUploadTaskRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(TestUploadTaskRequest)

    Q_SERIALIZE_PROPERTY(int, code);
    Q_SERIALIZE_PROPERTY(QStringList, objects);
    Q_SERIALIZE_PROPERTY(linglong::repo::TestPairList, refList);
    Q_SERIALIZE_PROPERTY(linglong::repo::TestPairStrMap, refs);
};

} // namespace repo
} // namespace linglong

Q_SERIALIZE_DECLARE_METATYPE_NM(linglong::repo, TestUploadTaskRequest)
#endif
