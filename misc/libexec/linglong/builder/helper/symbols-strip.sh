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
        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugIDFile")"
        mkdir -p "$(dirname "$debugFile")"
        tmp_strip=$(mktemp "$prefix/strip.XXXXXX")
        tmp_debug=$(mktemp "$prefix/debug.XXXXXX")
        cp -p "$filepath" "$tmp_strip"
        eu-strip "$tmp_strip" -f "$tmp_debug"

        readelf_output=$(readelf -l "$tmp_strip" 2>/dev/null)
        total=$(echo "$readelf_output" | grep "There are" | awk '{print $3}')
        if [ -n "$total" ] && [ "$total" -ne 0 ]; then
                null_count=$(echo "$readelf_output" | grep -c "^  NULL")
                if [ "$total" -eq "$null_count" ]; then
                        rm -f "$tmp_strip" "$tmp_debug"
                        objcopy --only-keep-debug "$filepath" "$debugIDFile" || true
                        [ -s "$debugIDFile" ] || continue
                        ln -sf "$debugIDFile" "$debugFile"
                        continue
                fi
        fi
        mv "$tmp_strip" "$filepath"
        mv "$tmp_debug" "$debugIDFile"

        echo "striped $filepath to $debugIDFile"
        ln -sf "$debugIDFile" "$debugFile"
done <"$tmpFile"
