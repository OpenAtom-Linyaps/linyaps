/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/util/serialize/serialize.h"
#include "module/util/json.h"

namespace linglong {
namespace repo {

class Pair : public JsonSerialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Pair);

public:
    Q_SERIALIZE_PROPERTY(QString, first);
    Q_SERIALIZE_PROPERTY(QString, second);
};

} // namespace repo
} // namespace linglong

Q_SERIALIZE_DECLARE_METATYPE_NM(linglong::repo, Pair)

namespace linglong {
namespace repo {

class UploadTaskRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(UploadTaskRequest)

    Q_SERIALIZE_PROPERTY(int, code);
    Q_SERIALIZE_PROPERTY(QStringList, objects);
    Q_SERIALIZE_PROPERTY(linglong::repo::PairList, refList);
    Q_SERIALIZE_PROPERTY(linglong::repo::PairStrMap, refs);
};

} // namespace repo
} // namespace linglong

Q_SERIALIZE_DECLARE_METATYPE_NM(linglong::repo, UploadTaskRequest)