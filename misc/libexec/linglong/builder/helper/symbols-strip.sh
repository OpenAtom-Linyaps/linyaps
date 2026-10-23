#!/bin/bash

# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -euo pipefail

prefix="${PREFIX:?PREFIX is required}"

tmpFile=$(mktemp)
trap 'rm -f "$tmpFile"' EXIT

find "$prefix" -type f >"$tmpFile"

while read -r filepath; do
        fileinfo=$(file "$filepath")
        # skip stripped
        if ! echo "$fileinfo" | grep -q 'not stripped'; then
                continue
        fi
        # skip debug file
        if [[ "$filepath" == *.debug ]]; then
                continue
        fi
        # https://sourceware.org/gdb/current/onlinedocs/gdb.html/Separate-Debug-Files.html
        # awk exits 0 when no line matches, so a missing Build ID yields an empty string.
        buildID=$(readelf -n "$filepath" 2>/dev/null | awk '/Build ID/ {print $NF}')
        if [[ -z "${buildID}" ]]; then
                echo "skip $filepath: no Build ID found"
                continue
        fi
        debugIDFile="$prefix/lib/debug/.build-id/${buildID:0:2}/${buildID:2}.debug"
        mkdir -p "$(dirname "$debugIDFile")"
        eu-strip "$filepath" -f "$debugIDFile"
        echo "stripped $filepath to $debugIDFile"

        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugFile")"
        ln -sfn "$debugIDFile" "$debugFile"
done <"$tmpFile"
