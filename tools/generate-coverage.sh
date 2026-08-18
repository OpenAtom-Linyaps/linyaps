#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e

cd "$(git rev-parse --show-toplevel)" || exit 255

builddir=build-generate-coverage

export CXXFLAGS="$CXXFLAGS --coverage -g -O0"

command -v ccache &> /dev/null && {
    USE_CCACHE=-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
}

# shellcheck disable=SC2086
cmake --fresh -B "$builddir" -S . "$USE_CCACHE" || exit 255

NUM_JOBS=${NUM_JOBS:-$(nproc)}
cmake --build "$builddir" -j "$NUM_JOBS" || exit 255

# 用cmake会执行多次SetUpTestSuite
# cmake --build "$builddir" -t test -- ARGS="--output-on-failure"

# Run the unit tests, but do not abort the script if some tests fail: the
# coverage data (.gcda) is still written, so keep going and generate the
# report. A non-zero test exit code is reported at the end of the script.
set +e
"$builddir/libs/linglong/tests/ll-tests/ll-tests"
test_ret=$?
set -e
if [ "$test_ret" -ne 0 ]; then
    echo "WARNING: ll-tests exited with code ${test_ret}; generating coverage report anyway" >&2
fi

mkdir -p "$builddir"/report || exit 255

gcovr \
    --filter "apps/.*" \
    --filter "libs/common/.*" \
    --filter "libs/oci-cfg-generators/.*" \
    --filter "libs/utils/.*" \
    --filter "libs/linglong/src/.*" \
    --html-nested "$builddir"/report/index.html \
    --xml "$builddir"/report/index.xml \
    --print-summary

if command -v xdg-open &> /dev/null; then
    echo "use xdg-open $builddir/report/index.html to view coverage report"
else
    echo "Open $builddir/report/index.html in your web browser to view the coverage report."
fi

exit "$test_ret"
