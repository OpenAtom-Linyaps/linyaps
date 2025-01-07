// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "10_basics.h"

#include "ocppi/runtime/config/types/Generators.hpp"

#include <iostream>

namespace linglong::generator {

bool Basics::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    nlohmann::json rawPatch;

    try {
        rawPatch = nlohmann::json::parse(basicsPatch);
    } catch (const std::exception &e) {
        std::cerr << "parse basicsPatch failed:" << e.what() << std::endl;
        return false;
    }

    if (config.ociVersion != rawPatch["ociVersion"].get<std::string>()) {
        std::cerr << "ociVersion mismatched" << std::endl;
        return false;
    }

    if (!config.annotations) {
        std::cerr << "no annotations." << std::endl;
        return false;
    }

    auto onlyApp = config.annotations->find("org.deepin.linglong.onlyApp");
    if (onlyApp != config.annotations->end() && onlyApp->second == "true") {
        return true;
    }

    try {
        auto rawConfig = nlohmann::json(config);
        rawConfig = rawConfig.patch(rawPatch["patch"]);
        config = rawConfig.get<ocppi::runtime::config::types::Config>();
    } catch (const std::exception &e) {
        std::cerr << "patch basics failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

} // namespace linglong::generator
