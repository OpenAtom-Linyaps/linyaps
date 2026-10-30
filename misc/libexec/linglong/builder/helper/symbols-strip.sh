#!/bin/bash

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eo pipefail

prefix="${PREFIX:?PREFIX must be set}"

tmpFile=$(mktemp)
trap 'rm -f "$tmpFile"' EXIT

find "$prefix" -type f > "$tmpFile"

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
    if buildID=$(readelf -n "$filepath" | awk '/Build ID/ {print $NF}'); then
        :
    else
        status=$?
        echo "failed to read Build ID from $filepath" >&2
        exit "$status"
    fi
    # Without an ID there is no valid .build-id destination. Leave the
    # original ELF intact rather than producing an unusable debug file.
    if [[ -z "$buildID" ]]; then
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
done < "$tmpFile"
