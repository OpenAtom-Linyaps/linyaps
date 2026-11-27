// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <optional>
#include <string>

namespace linglong::cdi_drv {

namespace types {
struct CdiDrvConfig;
}

struct GeneratedFiles
{
    std::string specFilename;
    std::string spec;
    std::string config;
};

utils::error::Result<std::optional<GeneratedFiles>> generate(const std::string &name,
                                                             const types::CdiDrvConfig &config);

} // namespace linglong::cdi_drv
