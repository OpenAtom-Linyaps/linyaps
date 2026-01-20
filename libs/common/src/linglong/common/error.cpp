// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/error.h"

#include <system_error>

namespace linglong::common::error {

std::string errorString(int err)
{
    return std::system_category().message(err);
}

} // namespace linglong::common::error
