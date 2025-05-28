/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

// NOTE: DO NOT REMOVE THIS HEADER, nlohmann::json need this header to lookup function 'from_json'
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "ytj/ytj.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace linglong::utils::serialize {

template <typename T, typename Source>
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

template <typename T>
error::Result<T> LoadYAMLFile(const std::filesystem::path &filename) noexcept
{
    LINGLONG_TRACE("load yaml from file");

    std::ifstream file_stream(filename);
    if (!file_stream.is_open()) {
        return LINGLONG_ERR("Failed to open file: " + QString::fromStdString(filename));
    }

    return LoadYAML<T>(file_stream);
}

} // namespace linglong::utils::serialize
