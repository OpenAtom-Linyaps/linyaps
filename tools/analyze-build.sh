#!/bin/env bash

# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Build linyaps with Clang -ftime-trace and produce a build-time hot-spot
# report via clang-build-analyzer.
#
# Usage: tools/analyze-build.sh
#
# Prerequisites: clang/clang++ and the usual linyaps build dependencies.
# clang-build-analyzer is built from source into ~/.cache on first run if it
# is not already on PATH.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)" || exit 255

builddir=build-trace
analyzer_root="${HOME}/.cache/clang-build-analyzer"
analyzer_bin="${analyzer_root}/clang-build-analyzer"

ensure_analyzer() {
    if command -v clang-build-analyzer &> /dev/null; then
        return 0
    fi
    if [ -x "$analyzer_bin" ]; then
        return 0
    fi

    echo "clang-build-analyzer not found, building from source..." >&2
    mkdir -p "$analyzer_root"
    local src="$analyzer_root/src"
    if [ ! -d "$src/.git" ]; then
        git clone --depth 1 https://github.com/CacheFactory/clang-build-analyzer "$src"
    fi
    cmake -S "$src" -B "$analyzer_root/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$analyzer_root/build" -j "$(nproc)"
    cp "$analyzer_root/build/clang-build-analyzer" "$analyzer_bin"
}

ensure_analyzer

analyzer=clang-build-analyzer
if ! command -v clang-build-analyzer &> /dev/null; then
    analyzer="$analyzer_bin"
fi

echo "==> Configuring (preset build-trace)..."
cmake --fresh --preset build-trace

echo "==> Building..."
num_jobs=${NUM_JOBS:-$(nproc)}
cmake --build --preset build-trace -j "$num_jobs"

echo "==> Aggregating -ftime-trace files..."
report="${builddir}/build-analysis.json"
"$analyzer" --all "$builddir" "$report"

echo "==> Report written to ${report}"
echo
"$analyzer" --analyze "$report"
