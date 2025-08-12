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
cmake --fresh -B "$builddir" -S . "$USE_CCACHE" || exit 255

NUM_JOBS=${NUM_JOBS:-$(nproc)}
cmake --build "$builddir" -j "$NUM_JOBS" || exit 255

# 用cmake会执行多次SetUpTestSuite
# cmake --build "$builddir" -t test -- ARGS="--output-on-failure"

$builddir/libs/linglong/tests/ll-tests/ll-tests

mkdir -p "$builddir"/report || exit 255

gcovr \
    --filter "apps/.*" \
    --filter "libs/utils/.*" \
    --filter "libs/linglong/src/.*" \
    --html-nested "$builddir"/report/index.html \
    --xml "$builddir"/report/index.xml \
    --print-summary

if command -v xdg-open &>/dev/null; then
    echo "use xdg-open $builddir/report/index.html to view coverage report"
else
    echo "Open $builddir/report/index.html in your web browser to view the coverage report."
fi