// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "config.h"

#include "linglong/cdi_drv/types/Generators.hpp"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/json.h"

#include <utility>

namespace linglong::cdi_drv {

bool validConfigName(std::string_view name)
{
    return !name.empty() && name.front() != '.'
      && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")
      == std::string_view::npos;
}

utils::error::Result<std::set<std::string>> configNames(const std::filesystem::path &configDir,
                                                        const RefreshRequests &requests)
{
    LINGLONG_TRACE("select CDI driver configurations");

    namespace fs = std::filesystem;
    auto names = requests.names;
    for (const auto &name : names) {
        if (!validConfigName(name)) {
            return LINGLONG_ERR("invalid CDI configuration name: " + name);
        }
    }
    std::error_code ec;
    if (requests.all) {
        fs::directory_iterator entries(configDir, ec);
        if (ec == std::errc::no_such_file_or_directory) {
            return std::set<std::string>{};
        }
        while (!ec && entries != fs::directory_iterator{}) {
            const auto path = entries->path();
            if (path.extension() == ".json") {
                const auto name = path.stem().string();
                if (validConfigName(name)) {
                    names.insert(name);
                } else if (name.front() != '.') {
                    LogW("Skipping CDI source {} because its configuration name is invalid", path);
                }
            }
            entries.increment(ec);
        }
        if (ec) {
            return LINGLONG_ERR("enumerate CDI driver configurations", ec);
        }
    }

    return names;
}

utils::error::Result<std::optional<types::CdiDrvConfig>>
loadConfig(const std::filesystem::path &path)
{
    LINGLONG_TRACE("load CDI driver configuration");

    namespace fs = std::filesystem;
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return LINGLONG_ERR("check CDI driver configuration", ec);
    }
    if (!fs::is_regular_file(status)) {
        if (fs::exists(status)) {
            LogE("Skipping CDI source {} because it is not a regular file", path);
        }
        return LINGLONG_OK;
    }
    auto source = utils::readFile(path);
    if (!source) {
        return LINGLONG_ERR(source);
    }
    auto config = utils::serialize::LoadJSON<types::CdiDrvConfig>(*source);
    if (!config) {
        LogE("Skipping CDI source {}: {}", path, config.error().message());
        return LINGLONG_OK;
    }
    return std::optional{ std::move(*config) };
}

} // namespace linglong::cdi_drv
