#!/bin/bash

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"

tmpfile=$(mktemp cmake/PFL.cmake.XXXXXX)
trap 'rm -f "$tmpfile"' EXIT
wget -O "$tmpfile" https://github.com/black-desk/PFL.cmake/releases/latest/download/PFL.cmake
mv "$tmpfile" cmake/PFL.cmake
trap - EXIT
