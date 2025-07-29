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

zip_file="$tmpdir"/external/github-TartanLlama-expected-v1.2.0.zip

wget https://github.com/TartanLlama/expected/archive/refs/tags/v1.2.0.zip \
        -O "$zip_file" || exit 255

rm -rf external/tl-expected

unzip "$zip_file" || exit 255

mkdir -p external/tl-expected || exit 255
mv expected-1.2.0/include external/tl-expected || exit 255
rm -rf expected-1.2.0 || exit 255

cli11_file="$tmpdir"/external/CLI.hpp

wget https://github.com/CLIUtils/CLI11/releases/download/v2.5.0/CLI11.hpp \
        -O "$cli11_file" || exit 255

rm -rf external/CLI11

mkdir -p external/CLI11/CLI || exit 255

mv "$cli11_file" external/CLI11/CLI/CLI.hpp || exit 255

rm -rf "$tmpdir"
