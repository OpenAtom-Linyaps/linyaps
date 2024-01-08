#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e
set -x

create_files() {
        tmpdir=$1
        shift

        mkdir -p "$tmpdir"
        files=("$tmpdir"/test_file.txt)
        for file in "${files[@]}"; do
                touch "$file"
        done
        echo '{
  "appid": "org.deepin.test",
  "arch": ["x86"],
  "version": "1.0.0",
  "module": "runtime"
}' > "$tmpdir"/info.json
        find "$tmpdir" -type f
}

check_commit() {
        repo=$1
        shift
        ref=$1
        shift
        file=$1
        shift

        ostree --repo="$repo" ls "$ref" "$file" --nul-filenames-only | strings
}

check_files() {
        for file in "$@"; do
                test -f ."$file"
        done
}

"$@"
