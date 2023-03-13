/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
#define LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_

// yaml/json format config

#include "module/util/file.h"
#include "module/util/qserializer/deprecated.h"

namespace linglong {

class Config : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Config)
    Q_JSON_PROPERTY(QString, repoUrl);

    inline static QString path() { return util::getLinglongRootPath() + "/config.json"; }
};

inline Config &ConfigInstance()
{
    static QSharedPointer<Config> config(util::loadJson<Config>(Config::path()));
    return *config;
}

} // namespace linglong

#endif // LINGLONG_SRC_MODULE_UTIL_CONFIG_CONFIG_H_
