/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/error/error.h"

namespace linglong::utils::error {

bool isVerboseMessageEnabled()
{
    static bool cached = []() {
        const char *verboseEnv = std::getenv("LINGLONG_VERBOSE_MESSAGE");
        return (verboseEnv != nullptr && QString(verboseEnv).toLower() == "true");
    }();
    return cached;
}

} // namespace linglong::utils::error
