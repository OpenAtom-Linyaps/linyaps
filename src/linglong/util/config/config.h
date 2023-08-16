/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTIL_CONFIG_CONFIG_H_
#define LINGLONG_UTIL_CONFIG_CONFIG_H_

// yaml/json format config

#include "linglong/repo/repo.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/deprecated.h"
#include "linglong/util/qserializer/json.h"

namespace linglong::config {

class Repo : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Repo)
public:
    Q_SERIALIZE_PROPERTY(QString, endpoint);
    Q_SERIALIZE_PROPERTY(QString, repoName);
};

class Config : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Config)

public:
    Q_PROPERTY(QMap<QString, QSharedPointer<linglong::config::Repo>> repos MEMBER repos);
    QMap<QString, QSharedPointer<config::Repo>> repos;

public:
    void save();

private:
    friend QSharedPointer<config::Config> loadConfig();
    QString path;

    // TODO: it so strange that a global config could not serialize self
    QWeakPointer<Config> self;
};

QSERIALIZER_DECLARE(Repo)
QSERIALIZER_DECLARE(Config)

Config &ConfigInstance();

} // namespace linglong::config

#endif // LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
