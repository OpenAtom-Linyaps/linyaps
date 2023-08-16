/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTIL_CONFIG_REPO_H_
#define LINGLONG_UTIL_CONFIG_REPO_H_

#include "linglong/util/qserializer/deprecated.h"

namespace linglong::util::config {

class Repo : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Repo);
public:
    Q_SERIALIZE_PROPERTY(QString, endpoint);
    Q_SERIALIZE_PROPERTY(QString, repoName);
};

QSERIALIZER_DECLARE(Repo);

} // namespace linglong::util::config
#endif
