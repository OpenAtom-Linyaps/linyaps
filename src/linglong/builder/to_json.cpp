/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/linglong_builder.h"
#include "ocppi/runtime/config/types/Generators.hpp"

namespace linglong::builder {

auto LinglongBuilder::toJSON(const ocppi::runtime::config::types::Mount &m) -> nlohmann::json
{
    return m;
}

auto LinglongBuilder::toJSON(const ocppi::runtime::config::types::Config &cfg) -> nlohmann::json
{
    return cfg;
}

} // namespace linglong::builder
