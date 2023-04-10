/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "module/package/ref.h"
#include "module/util/file.h"
#include "module/util/qserializer/yaml.h"

#include <fstream>
#include <mutex>

namespace linglong {
namespace config {

QSERIALIZER_IMPL(Repo);
QSERIALIZER_IMPL(Config);

static const char *const kConfigFileName = "config.yaml";
// TODO: check more path for different distribution
static const char *const kDefaultConfigFilePath = DATADIR "/linglong/config.yaml";

static QString filePath()
{
    return util::getLinglongRootPath() + "/" + kConfigFileName;
}

static QSharedPointer<linglong::config::Config> loadConfig()
{
    auto configFilePath = filePath();
    if (!util::fileExists(configFilePath) && util::fileExists(kDefaultConfigFilePath)) {
        configFilePath = kDefaultConfigFilePath;
    }

    QSharedPointer<linglong::config::Config> cfg;
    try {
        cfg = std::get<0>(
                linglong::util::fromYAML<QSharedPointer<linglong::config::Config>>(filePath()));
    } catch (...) {
        qInfo() << "Failed to load config, cfg:" << cfg;
    }

    if (!cfg) {
        cfg = QSharedPointer<linglong::config::Config>(new Config);
    }

    qCritical() << cfg;
    qCritical() << cfg->repos.keys();
    qCritical() << cfg->repos[package::kDefaultRepo]->endpoint;
    Q_ASSERT(cfg);
    if (!cfg->repos.contains(package::kDefaultRepo)) {
        cfg->repos.insert(package::kDefaultRepo,
                          QSharedPointer<config::Repo>(new Repo(cfg.data())));
    };

    Q_ASSERT(cfg->repos.contains(package::kDefaultRepo));

    return cfg;
}

Config::Config(QObject *parent)
    : Serialize(parent)
{
}

// void Config::save()
//{
//     auto node = toYaml<Config>(this);
//     std::ofstream configPathOut(filePath().toStdString());
//     configPathOut << node;
//     configPathOut.close();
// }

} // namespace config
} // namespace linglong

linglong::config::Config &ConfigInstance()
{
    static QSharedPointer<linglong::config::Config> config(linglong::config::loadConfig());
    return *config;
}
