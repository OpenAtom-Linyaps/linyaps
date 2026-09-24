/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#ifndef LINGLONG_CLI_TRANSFORM_OLD_EXEC_H_
#define LINGLONG_CLI_TRANSFORM_OLD_EXEC_H_

#include <string>
#include <vector>

// Rewrites legacy ll-cli `--exec` separators to `--`.
// The returned vector is in reverse argv order (without argv[0]), matching
// CLI::App::parse(std::vector<std::string>&).
std::vector<std::string> transformOldExec(int argc, char **argv) noexcept;

#endif
