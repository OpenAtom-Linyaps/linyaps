/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

// NOTE: This cpp file is used by Findnlohmann_json.cmake to check
// whether the nlohmann_json library we founded at include directory
// is compatible with 3.5.0 or not.

#if NLOHMANN_JSON_VERSION_MAJOR != 3
#  error require nlohmann_json compatible with 3.5.0
#endif

#if NLOHMANN_JSON_VERSION_MINOR < 5
#  error require nlohmann_json compatible with 3.5.0
#endif

int main()
{
    return 0;
}
