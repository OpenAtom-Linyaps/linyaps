#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

prefix="$PREFIX"

tmpFile=$(mktemp)

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
        buildID=$(readelf -n "$filepath" | grep 'Build ID' | awk '{print $NF}')
        debugIDFile="$prefix/lib/debug/.build-id/${buildID:0:2}/${buildID:2}.debug"
        mkdir -p "$(dirname "$debugIDFile")"

        tmp_strip=$(mktemp)
        tmp_debug=$(mktemp)
        cp "$filepath" "$tmp_strip"
        eu-strip "$tmp_strip" -f "$tmp_debug"

        total=$(readelf -l "$tmp_strip" 2>/dev/null | grep "There are" | awk '{print $3}')
        null_count=$(readelf -l "$tmp_strip" 2>/dev/null | grep -c "^  NULL")
        if [ -n "$total" ] && [ "$total" -eq "$null_count" ] && [ "$total" -ne 0 ]; then
                echo "invalid program headers after stripping, skipping: $filepath"
                rm -f "$tmp_strip" "$tmp_debug"
                continue
        fi
        mv "$tmp_strip" "$filepath"
        mv "$tmp_debug" "$debugIDFile"

        echo "striped $filepath to $debugIDFile"

        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugFile")"
        ln -sf "$debugIDFile" "$debugFile"
done <"$tmpFile"
