// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <string>

namespace linglong::utils {

// quote the bash argument to avoid the space in the argument
// and use single quote to avoid the shell to expand the argument
// example:
// arg: "let's go"
// quoted arg: "'let'\''s go'"
static inline std::string quoteBashArg(std::string arg) noexcept
{
    const std::string quotePrefix = "'\\";
    for (auto it = arg.begin(); it != arg.end(); it++) {
        if (*it == '\'') {
            it = arg.insert(it, quotePrefix.cbegin(), quotePrefix.cend());
            it = arg.insert(it + quotePrefix.size() + 1, 1, '\'');
        }
    }
    return "'" + arg + "'";
}

} // namespace linglong::utils