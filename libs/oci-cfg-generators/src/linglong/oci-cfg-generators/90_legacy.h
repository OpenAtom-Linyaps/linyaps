// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/oci-cfg-generators/generator.h"

namespace linglong::generator {
class Legacy : public Generator
{
public:
    [[nodiscard]] std::string_view name() const override { return "90-legacy"; }

    bool generate(ocppi::runtime::config::types::Config &config) const noexcept override;
};
} // namespace linglong::generator
