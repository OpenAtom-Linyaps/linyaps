/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/config.h"

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/yaml.h"

#include <fstream>

namespace linglong::builder {

auto loadConfig(const QString &file) noexcept -> utils::error::Result<api::types::v1::BuilderConfig>
{
    LINGLONG_TRACE(QString("load build config from %1").arg(file));

    try {
        QFile f(file);
        qDebug() << "read build config file" << file;
        if (!f.open(QIODevice::ReadOnly)) {
            return LINGLONG_ERR("read build config file", f);
        }
        auto data = f.readAll().toStdString();
        auto config = utils::serialize::LoadYAML<api::types::v1::BuilderConfig>(data);
        if (!config) {
            return LINGLONG_ERR("parse build config", config);
        }
        if (config->version != 1) {
            return LINGLONG_ERR(
              QString("wrong configuration file version %1").arg(config->version));
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

auto loadConfig(const QStringList &files) noexcept
  -> utils::error::Result<api::types::v1::BuilderConfig>
{
    LINGLONG_TRACE(QString("load build config from %1").arg(files.join(" ")));

    for (const auto &file : files) {
        auto config = loadConfig(file);
        if (!config.has_value()) {
            qDebug() << "Failed to load build config from" << file << ":" << config.error();
            continue;
        }

        qDebug() << "Load build config from" << file;
        return config;
    }

    return LINGLONG_ERR("all failed");
}

auto saveConfig(const api::types::v1::BuilderConfig &cfg,
                const QString &path) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE(QString("save config to %1").arg(path));

    try {
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
} // namespace linglong::builder
