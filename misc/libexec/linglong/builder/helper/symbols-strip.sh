#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

prefix="$PREFIX"

tmpFile=$(mktemp)

find "$prefix" -type f > "$tmpFile"

while read -r filepath; do
    if file "$filepath" | grep -q 'not stripped'; then
        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugFile")"
        eu-strip "$filepath" -f "$debugFile"
        echo "striped $filepath"
    fi
done < "$tmpFile"
