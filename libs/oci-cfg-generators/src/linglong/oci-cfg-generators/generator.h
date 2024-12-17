// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "ocppi/runtime/config/types/Config.hpp"

namespace linglong::generator {

using string_list = std::vector<std::string>;

struct Generator
{
    Generator() = default;
    virtual ~Generator() = default;
    virtual bool generate(ocppi::runtime::config::types::Config &config) const noexcept = 0;

    [[nodiscard]] virtual std::string_view name() const = 0;
};
} // namespace linglong::generator
