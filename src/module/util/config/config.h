/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
#define LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_

// yaml/json format config

#include "module/repo/repo.h"
#include "module/util/file.h"
#include "module/util/qserializer/deprecated.h"
#include "module/util/qserializer/json.h"

namespace linglong {
namespace config {

class Repo : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Repo)
public:
    Q_SERIALIZE_PROPERTY(QString, endpoint);
};

class Config : public Serialize
{
    Q_OBJECT;

public:
    Q_PROPERTY(QMap<QString, QSharedPointer<linglong::config::Repo>> repos MEMBER repos);
    QMap<QString, QSharedPointer<linglong::config::Repo>> repos;

public:
    explicit Config(QObject *parent = nullptr);
    //    virtual void onPostSerialize() override;
    //    void save();
};

} // namespace config
} // namespace linglong

linglong::config::Config &ConfigInstance();

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::config, Repo)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::config, Config)

#endif // LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
