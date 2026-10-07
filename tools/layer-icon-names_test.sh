#!/usr/bin/env bash

# SPDX-FileCopyrightText: None
# SPDX-License-Identifier: CC0-1.0

set -e

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
checker="${LAYER_ICON_CHECKER:-${script_dir}/layer-icon-checker.sh}"
test_root=$(mktemp -d "${TMPDIR:-/tmp}/layer-icon-names.XXXXXXXX")
trap 'rm -rf "${test_root}"' EXIT
printf 'Fixture root: %s\n' "${test_root}"

# Load the actual checker functions without invoking its FUSE-dependent main.
source <(sed '/^main "\$@"$/d' "${checker}")

run_case() (
    case_name=$1
    expected=$2
    icon_name=$3
    shift 3
    layer="${test_root}/${case_name}"
    mkdir -p "${layer}/entries/share/applications" "${layer}/entries/share/icons/hicolor/scalable/apps"
    printf '%s\n' '[Desktop Entry]' 'Type=Application' 'Name=Probe' 'Exec=probe' \
        "Icon=${icon_name}" > "${layer}/entries/share/applications/probe.desktop"
    for file in "$@"; do
        printf '%s\n' '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128"><rect width="128" height="128"/></svg>' \
            > "${layer}/entries/share/icons/hicolor/scalable/apps/${file}"
    done
    icons=$(getDesktopIcons "${layer}")
    iconCheck "${layer}" "${icons}" > "${layer}/stdout.log" 2> "${layer}/stderr.log"
    actual=$?
    if [ "${actual}" -ne "${expected}" ] || [ -s "${layer}/stderr.log" ]; then
        printf 'FAIL %s: expected exit %s, actual %s\n' "${case_name}" "${expected}" "${actual}"
        cat "${layer}/stdout.log" "${layer}/stderr.log"
        exit 1
    fi
    printf 'PASS %s: exit %s\n' "${case_name}" "${actual}"
)

failures=0
run_case exact-svg 0 qbittorrent qbittorrent.svg || failures=$((failures + 1))
run_case absent 255 qbittorrent || failures=$((failures + 1))
run_case hyphen-prefix 255 qbittorrent qbittorrent-extra.svg || failures=$((failures + 1))
run_case dotted-prefix 255 org.example.App org.example.App.helper.svg || failures=$((failures + 1))
run_case dotted-exact 0 org.example.App org.example.App.svg || failures=$((failures + 1))
run_case explicit-extension 0 qbittorrent.svg qbittorrent.svg || failures=$((failures + 1))
run_case no-extension 0 qbittorrent qbittorrent || failures=$((failures + 1))
run_case extension-agnostic 0 qbittorrent qbittorrent.custom || failures=$((failures + 1))
run_case mixed-exact-and-prefix 0 qbittorrent qbittorrent.svg qbittorrent-extra.svg || failures=$((failures + 1))
printf '9 cases, %s failures\n' "${failures}"
[ "${failures}" -eq 0 ]
