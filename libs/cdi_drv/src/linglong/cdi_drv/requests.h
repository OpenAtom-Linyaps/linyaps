// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <set>
#include <string>

namespace linglong::cdi_drv {

struct RefreshRequests
{
    bool all{ false };
    std::set<std::string> names;
};

} // namespace linglong::cdi_drv
