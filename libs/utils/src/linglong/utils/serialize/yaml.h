/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_SERIALIZE_YAML_H
#define LINGLONG_UTILS_SERIALIZE_YAML_H

// NOTE: DO NOT REMOVE THIS HEADER, nlohmann::json need this header to lookup function 'from_json'
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "ytj/ytj.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <exception>

namespace linglong::utils::serialize {

template<typename T, typename Source>
error::Result<T> LoadYAML(Source &content)
{
    LINGLONG_TRACE("load yaml");
    try {
        YAML::Node node = YAML::Load(content);
        nlohmann::json json = ytj::to_json(node);
        return json.template get<T>();
    } catch (...) {
        return LINGLONG_ERR(std::current_exception());
    }
}

template<typename T>
error::Result<T> LoadYAMLFile(const QString &filename) noexcept
{
    LINGLONG_TRACE("load yaml from file");

    QFile file = filename;

    file.open(QFile::ReadOnly);
    if (!file.isOpen()) {
        return LINGLONG_ERR("open", file);
    }

    Q_ASSERT(file.error() == QFile::NoError);

    auto content = file.readAll();
    if (file.error() != QFile::NoError) {
        return LINGLONG_ERR("read all", file);
    }

    return LoadYAML<T>(content);
}

} // namespace linglong::utils::serialize

#endif
