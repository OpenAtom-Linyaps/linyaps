/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "transform_old_exec.h"

#include <string_view>

std::vector<std::string> transformOldExec(int argc, char **argv) noexcept
{
    if (!argv || argc <= 0) {
        return {};
    }

    std::vector<std::string> res;

    for (int i = argc - 1; i > 0; --i) {
        if (!argv[i]) {
            continue;
        }
        if (std::string_view(argv[i]) == "--exec") {
            res.emplace_back("--");
        } else {
            res.emplace_back(argv[i]);
        }
    }

    return res;
}
