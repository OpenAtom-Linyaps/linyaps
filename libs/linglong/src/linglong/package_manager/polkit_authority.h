// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <string>

namespace linglong::service {

class PolkitAuthority
{
public:
    PolkitAuthority() = delete;

    static utils::error::Result<bool> checkAuthorization(const std::string &actionId,
                                                         const std::string &systemBusName,
                                                         bool userInteraction = true);
};

} // namespace linglong::service
