/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app.h"
#include "ocppi/runtime/config/types/Generators.hpp"

namespace linglong::runtime {

auto App::toJSON(const ocppi::runtime::config::types::Config &cfg) -> nlohmann::json
{
    return cfg;
}

auto App::toJSON(const ocppi::runtime::config::types::Process &p) -> nlohmann::json
{
    return p;
}
} // namespace linglong::runtime
