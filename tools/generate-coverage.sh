#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e

cd "$(git rev-parse --show-toplevel)" || exit 255

builddir=build-generate-coverage

export CXXFLAGS="$CXXFLAGS --coverage -g -O0"

command -v ccache &>/dev/null && {
        USE_CCACHE=-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
}

# shellcheck disable=SC2086
cmake --fresh -B "$builddir" -S . $USE_CCACHE || exit 255

cmake --build "$builddir" -j "$(nproc)" || exit 255

cmake --build "$builddir" -t test

mkdir -p "$builddir"/report || exit 255

gcovr --filter "src/*" --html-nested "$builddir"/report/index.html
