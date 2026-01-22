/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

// NOTE: DO NOT REMOVE THIS HEADER, nlohmann::json need this header to lookup function 'from_json'
#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/utils/error/error.h"
#include "nlohmann/json.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>

namespace linglong::utils::serialize {

template <typename T, typename Source>
error::Result<T> LoadJSON(const Source &content) noexcept
{
    LINGLONG_TRACE("load json");

    try {
        if constexpr (std::is_same_v<Source, nlohmann::basic_json<>>) {
            return content.template get<T>();
        } else {
            const auto &json = nlohmann::json::parse(const_cast<Source &>(content)); // NOLINT
            return json.template get<T>();
        }
    } catch (const std::exception &e) {
        return LINGLONG_ERR("failed to parse json", e);
    }
}

template <typename T>
error::Result<T> LoadJSONFile(const std::filesystem::path &filePath) noexcept
{
    LINGLONG_TRACE(fmt::format("load json from {}", filePath.string()));
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return LINGLONG_ERR("failed to open file");
    }

    return LoadJSON<T>(file);
}

} // namespace linglong::utils::serialize
