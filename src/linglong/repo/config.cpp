/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/repo/config.h"

#include "linglong/repo/config/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "ytj/ytj.hpp"

#include <fstream>

namespace linglong::repo {
using namespace config;

using namespace utils::error;

Result<ConfigV1> loadConfig(const QString &file) noexcept
{
    LINGLONG_TRACE(QString("load config from %1").arg(file));

    try {
        auto ifs = std::ifstream(file.toLocal8Bit());
        if (!ifs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto config = ytj::to_json(YAML::Load(ifs)).get<ConfigV1>();
        if (config.version != 1) {
            return LINGLONG_ERR(QString("wrong configuration file version %1").arg(config.version));
        }

        if (config.repos.find(config.defaultRepo) == config.repos.end()) {
            return LINGLONG_ERR(QString("default repo not found in repos"));
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

Result<ConfigV1> loadConfig(const QStringList &files) noexcept
{
    LINGLONG_TRACE(QString("load config from %1").arg(files.join(" ")));

    for (const auto &file : files) {
        auto config = loadConfig(file);
        if (!config.has_value()) {
            qDebug() << "Failed to load config from" << file << ":" << config.error();
            continue;
        }

        qDebug() << "Load config from" << file;
        return config;
    }

    return LINGLONG_ERR("all failed");
}

Result<void> saveConfig(const ConfigV1 &cfg, const QString &path) noexcept
{
    LINGLONG_TRACE(QString("save config to %1").arg(path));

    try {
        if (cfg.repos.find(cfg.defaultRepo) == cfg.repos.end()) {
            return LINGLONG_ERR("default repo not found in repos");
        }

        auto ofs = std::ofstream(path.toLocal8Bit());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto node = ytj::to_yaml(cfg);
        ofs << node;

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

} // namespace linglong::repo
