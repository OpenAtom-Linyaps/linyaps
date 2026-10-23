#!/bin/sh
# SPDX-FileCopyrightText: 2026 project-nju-learner
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Run without a Linyaps installation: sh misc/tests/generate_xdg_data_dirs_test.sh
set -u

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
template="$script_dir/../lib/linglong/generate-xdg-data-dirs.sh"
configured_script=$(mktemp)
trap 'rm -f "$configured_script"' 0
trap 'exit 1' 1 2 3 15

configure() {
    sed -e 's|@LINGLONG_ROOT@|/var/lib/linglong|g' \
        -e "s|@LINGLONG_EXPORT_PATH@|$1|g" \
        "$template" > "$configured_script"
}

failed=0
check() {
    label=$1
    expected=$2
    shift 2
    actual=$("$@" sh -eu -c '. "$1"; . "$1"; printf "%s" "$XDG_DATA_DIRS"' sh "$configured_script")
    status=$?
    if [ "$status" -eq 0 ] && [ "$actual" = "$expected" ]; then
        printf 'PASS: %s\n' "$label"
    else
        printf 'FAIL: %s (exit %s)\n  expected: %s\n  actual: %s\n' \
            "$label" "$status" "$expected" "$actual"
        failed=$((failed + 1))
    fi
}

configure share
check 'unset XDG_DATA_DIRS with errexit and nounset' \
    '/usr/local/share:/usr/share:/var/lib/linglong/entries/share' \
    env -u XDG_DATA_DIRS
check 'empty XDG_DATA_DIRS with errexit and nounset' \
    '/usr/local/share:/usr/share:/var/lib/linglong/entries/share' \
    env XDG_DATA_DIRS=
check 'preserve existing paths and avoid duplicates on repeated sourcing' \
    '/opt/acme/share:/usr/share:/var/lib/linglong/entries/share' \
    env XDG_DATA_DIRS=/opt/acme/share:/usr/share
check 'already exported Linyaps path' \
    '/usr/share:/var/lib/linglong/entries/share' \
    env XDG_DATA_DIRS=/usr/share:/var/lib/linglong/entries/share

configure desktop
check 'custom export path keeps its priority' \
    '/var/lib/linglong/entries/desktop:/usr/share:/var/lib/linglong/entries/share' \
    env XDG_DATA_DIRS=/usr/share

printf '%s failed\n' "$failed"
[ "$failed" -eq 0 ]
