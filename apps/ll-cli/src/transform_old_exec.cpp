/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "transform_old_exec.h"

#include <string_view>

std::vector<std::string> transformOldExec(int argc, char **argv) noexcept
{
    if (argv == nullptr || argc <= 1) {
        return {};
    }

    // Arguments after an explicit `--` belong to the guest program and must
    // keep a literal `--exec`. When no explicit `--` is present, every
    // `--exec` is the legacy separator and should be rewritten.
    bool seenExplicitSeparator = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string_view(argv[i]) == "--") {
            seenExplicitSeparator = true;
            break;
        }
    }

    std::vector<std::string> res;
    res.reserve(static_cast<std::size_t>(argc) - 1U);

    // Iterate backwards: CLI::App::parse(std::vector) expects reverse order.
    bool afterExplicitSeparator = seenExplicitSeparator;
    for (int i = argc - 1; i > 0; --i) {
        if (argv[i] == nullptr) {
            continue;
        }
        const std::string_view arg{ argv[i] };
        if (arg == "--") {
            afterExplicitSeparator = false;
            res.emplace_back("--");
        } else if (arg == "--exec" && !afterExplicitSeparator) {
            res.emplace_back("--");
        } else {
            res.emplace_back(arg);
        }
    }

    return res;
}
