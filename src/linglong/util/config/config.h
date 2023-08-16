/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTIL_CONFIG_CONFIG_H_
#define LINGLONG_UTIL_CONFIG_CONFIG_H_

// yaml/json format config

#include "linglong/util/config/repo.h"
#include "linglong/util/qserializer/deprecated.h"

namespace linglong::util::config {

class Config : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Config);

public:
    Q_PROPERTY(QMap<QString, QSharedPointer<linglong::util::config::Repo>> repos MEMBER repos);
    QMap<QString, QSharedPointer<Repo>> repos;

public:
    void save();

private:
    friend QSharedPointer<Config> loadConfig();
    QString path;

    // TODO: it so strange that a global config could not serialize self
    QWeakPointer<Config> self;
};

QSERIALIZER_DECLARE(Config);

Config &ConfigInstance();

} // namespace linglong::util::config

#endif // LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
