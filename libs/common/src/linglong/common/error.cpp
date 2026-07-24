// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/error.h"

#include <array>
#include <cstring> // IWYU pragma: keep

namespace linglong::common::error {

std::string errorString(int err)
{
    std::array<char, 256> buf{ };
    return { strerror_r(err, buf.data(), buf.size()) };
}

} // namespace linglong::common::error
