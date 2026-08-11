/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace linglong::cli {

inline std::vector<std::string> transformOldExecArguments(int argc, char **argv) noexcept
{
    auto commandSeparator = argc;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{ argv[i] } == "--") {
            commandSeparator = i;
            break;
        }
    }

    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc - 1));
    for (int i = argc - 1; i > 0; --i) {
        if (i < commandSeparator && std::string_view{ argv[i] } == "--exec") {
            result.emplace_back("--");
        } else {
            result.emplace_back(argv[i]);
        }
    }

    return result;
}

} // namespace linglong::cli
