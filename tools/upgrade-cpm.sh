#!/bin/bash

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"

tmpfile=$(mktemp cmake/CPM.cmake.XXXXXX)
trap 'rm -f "$tmpfile"' EXIT
wget -O "$tmpfile" https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
mv "$tmpfile" cmake/CPM.cmake
trap - EXIT
