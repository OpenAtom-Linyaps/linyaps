#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cd "$(git rev-parse --show-toplevel)" || exit 255

tmpdir=$(
        mktemp -d /tmp/linglong-develop-XXXXXX || exit 255
)

mkdir -p "$tmpdir"/external || exit 255

zip_file="$tmpdir"/external/github-black-desk-qserializer-master.zip

pushd external || exit 255

wget https://codeload.github.com/black-desk/qserializer/zip/refs/heads/master \
        -O "$zip_file" || exit 255

rm -rf qserializer

unzip "$zip_file" || exit 255

mv qserializer-master qserializer || exit 255

zip_file="$tmpdir"/external/github-black-desk-qserializer-master.zip

wget https://codeload.github.com/black-desk/ytj/zip/refs/heads/master \
        -O "$zip_file" || exit 255

rm -rf ytj

unzip "$zip_file" || exit 255

mv ytj-master ytj || exit 255
