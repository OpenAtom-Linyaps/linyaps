#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cd "$(git rev-parse --show-toplevel)" || exit 255

tmpdir=$(
        mktemp -d /tmp/linglong-develop-XXXXXX || exit 255
)

mkdir -p "$tmpdir"/external || exit 255

zip_file="$tmpdir"/external/github-black-desk-ytj-master.zip

wget https://codeload.github.com/black-desk/ytj/zip/refs/heads/master \
        -O "$zip_file" || exit 255

rm -rf external/ytj

unzip "$zip_file" || exit 255

mv ytj-master external/ytj || exit 255

zip_file="$tmpdir"/external/github-TartanLlama-expected-v1.1.0.zip

wget https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.zip \
        -O "$zip_file" || exit 255

rm -rf external/tl-expected

unzip "$zip_file" || exit 255

mv expected-1.1.0 external/tl-expected || exit 255

wget https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.1.zip \
        -O "$zip_file" || exit 255

rm -rf external/CLI11

unzip "$zip_file" || exit 255

mv CLI11-2.4.1 external/CLI11 || exit 255
