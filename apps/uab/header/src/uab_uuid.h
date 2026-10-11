// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string_view>

namespace linglong::uab {

inline bool isUabUuidV4(std::string_view uuid) noexcept
{
    if (uuid.size() != 36 || uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-'
        || uuid[14] != '4') {
        return false;
    }

    const auto isHexDigit = [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')
          || (character >= 'A' && character <= 'F');
    };

    for (std::size_t i = 0; i < uuid.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }
        if (!isHexDigit(uuid[i])) {
            return false;
        }
    }

    const auto variant = uuid[19];
    return variant == '8' || variant == '9' || variant == 'a' || variant == 'b' || variant == 'A'
      || variant == 'B';
}

} // namespace linglong::uab
