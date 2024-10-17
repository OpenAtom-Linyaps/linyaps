#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

prefix="$PREFIX"

tmpFile=$(mktemp)

find "$prefix" -type f > "$tmpFile"

while read -r filepath; do
    if file "$filepath" | grep -q 'not stripped'; then
        # https://sourceware.org/gdb/current/onlinedocs/gdb.html/Separate-Debug-Files.html
        buildID=$(readelf -n "$filepath" | grep 'Build ID' | awk '{print $NF}')
        debugIDFile="$prefix/lib/debug/.build-id/${buildID:0:2}/${buildID:2}.debug"
        mkdir -p "$(dirname "$debugIDFile")"
        eu-strip "$filepath" -f "$debugIDFile"
        echo "striped $filepath to $debugIDFile" 

        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugFile")"
        ln -s "$debugIDFile" "$debugFile"
    fi
done < "$tmpFile"
