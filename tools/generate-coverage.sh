#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x

cd "$(git rev-parse --show-toplevel)" || exit 255

tmpdir=$(
        mktemp -d build-generate-coverage-XXXXXX || exit 255
)

export CXXFLAGS="$CXXFLAGS --coverage -g -O0"

cmake -B "$tmpdir" -S . || exit 255

cmake --build "$tmpdir" -j "$(nproc)" || exit 255

cmake --build "$tmpdir" -t test

mkdir "$tmpdir"/report || exit 255

gcovr --filter "src/*" --html-nested "$tmpdir"/report/index.html
