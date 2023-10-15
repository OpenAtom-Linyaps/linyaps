#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e

cd "$(git rev-parse --show-toplevel)" || exit 255

builddir=build-run-clang-tidy

command -v clang++ &>/dev/null
command -v clang-tidy &>/dev/null

command -v ccache &>/dev/null && {
        USE_CCACHE=-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
}

# shellcheck disable=SC2086
cmake --fresh -B "$builddir" -S . \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
        $USE_CCACHE || exit 255

cmake --build "$builddir" -j "$(nproc)" || exit 255
