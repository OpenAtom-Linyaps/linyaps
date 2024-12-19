// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "generator.h"

namespace linglong::generator {

struct Devices : public Generator
{
    [[nodiscard]] std::string_view name() const override { return "20-devices"; }

    bool generate(ocppi::runtime::config::types::Config &config) const noexcept override;
};

} // namespace linglong::generator