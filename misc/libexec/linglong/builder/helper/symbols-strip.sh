#!/bin/bash

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

prefix="$PREFIX"

tmpFile=$(mktemp)
trap 'rm -f "$tmpFile"' EXIT

find "$prefix" -type f >"$tmpFile"

# The generated entry script runs this helper under `set -e`, so a non-zero exit
# is what tells the build to stop. Report every failure and exit non-zero at the
# end rather than printing "striped" for a step that did not happen.
failed=0

while IFS= read -r filepath; do
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
        if ! eu-strip "$filepath" -f "$debugIDFile"; then
                echo "failed to strip $filepath" >&2
                failed=$((failed + 1))
                continue
        fi
        echo "striped $filepath to $debugIDFile"

        debugFile="$prefix/lib/debug/$filepath.debug"
        mkdir -p "$(dirname "$debugFile")"
        if ! ln -s "$debugIDFile" "$debugFile"; then
                echo "failed to link $debugFile to $debugIDFile" >&2
                failed=$((failed + 1))
        fi
done <"$tmpFile"

if [ "$failed" -ne 0 ]; then
        echo "symbols-strip: $failed step(s) failed" >&2
        exit 1
fi
