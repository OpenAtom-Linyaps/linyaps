// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <optional>
#include <string>

namespace linglong::ctk::detect {

class DeviceDetector
{
public:
    virtual ~DeviceDetector() = default;

    virtual utils::error::Result<std::optional<std::string>> detect() = 0;
};

} // namespace linglong::ctk::detect
