#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Hanabi9249
# SPDX-License-Identifier: LGPL-3.0-or-later
set -eo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
checker="${script_dir}/layer-icon-checker.sh"
test_root=$(mktemp -d "${TMPDIR:-/tmp}/layer-icon-filenames.XXXXXXXX")
trap 'rm -rf -- "${test_root}"' EXIT

# Load the real functions without running the FUSE mount entry point.
source <(sed '/^main "\$@"$/d' "${checker}")

write_desktop() {
    printf '%s\n' '[Desktop Entry]' 'Type=Application' 'Name=Probe' 'Exec=probe' "Icon=$2" >"$1"
}

mkdir "${test_root}/plain" "${test_root}/spaces" "${test_root}/mixed" "${test_root}/empty"
write_desktop "${test_root}/plain/app.desktop" first-icon
write_desktop "${test_root}/spaces/My App.desktop" first-icon
write_desktop "${test_root}/mixed/app.desktop" first-icon
write_desktop "${test_root}/mixed/My App.desktop" second-icon

assert_icons() {
    actual=$(getDesktopIcons "$1" | sed '/^$/d' | sort)
    if [ "${actual}" != "$2" ]; then
        printf 'Expected icons <%s>, got <%s> for %s\n' "$2" "${actual}" "$1" >&2
        return 1
    fi
}

assert_icons "${test_root}/plain" first-icon
assert_icons "${test_root}/spaces" first-icon
assert_icons "${test_root}/mixed" "$(printf '%s\n' first-icon second-icon)"
assert_icons "${test_root}/empty" ""
printf '%s\n' 'getDesktopIcons filename tests passed (4 cases)'
