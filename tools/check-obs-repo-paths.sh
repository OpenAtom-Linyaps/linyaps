#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Verify the OBS repository paths documented in the install guides are
# reachable and serve the expected indices, so a wrong path (for example an
# "xUbuntu_25.04" name that the OBS project does not publish for Ubuntu 25.x)
# is caught before it reaches users with a 404.
#
# The check hits the live https://ci.deepin.com mirror, so it needs network
# access and curl. It is meant to be run on demand whenever the repository
# paths in the install documents change:
#
#   ./tools/check-obs-repo-paths.sh
#
# Exit status: 0 when every documented path is reachable, non-zero otherwise.

set -Eeuo pipefail

RELEASE_BASE="https://ci.deepin.com/repo/obs/linglong:/CI:/release"
LATEST_BASE="https://ci.deepin.com/repo/obs/linglong:/CI:/latest"

FAILURES=0

http_code() {
    local url="$1"
    curl --silent --output /dev/null --max-time 20 --write-out '%{http_code}' "${url}"
}

expect_code() {
    local url="$1"
    local want="$2"
    local label="$3"
    local got
    got="$(http_code "${url}")"
    if [[ "${got}" == "${want}" ]]; then
        printf '  ok    %-7s %s\n' "${got}" "${label}"
    else
        printf '  FAIL  %-7s %s (expected %s)\n' "${got}" "${label}" "${want}"
        FAILURES=$((FAILURES + 1))
    fi
}

require_tool() {
    if ! command -v curl >/dev/null 2>&1; then
        printf 'This tool needs curl.\n' >&2
        exit 1
    fi
}

main() {
    require_tool

    local apt_paths=(
        Deepin_25
        Deepin_23
        Ubuntu_25.10
        Ubuntu_25.04
        xUbuntu_24.04
        Debian_13
        Debian_12
        uos_1070
        openkylin_2.0
    )
    local dnf_paths=(
        Fedora_42
        Fedora_43
        openEuler_25.03
        openEuler_24.03
        AnolisOS_23.4
        AnolisOS_23.3
    )

    printf 'Release channel: directories\n'
    for path in "${apt_paths[@]}" "${dnf_paths[@]}"; do
        expect_code "${RELEASE_BASE}/${path}/" "200" "release/${path}/"
    done

    printf 'Release channel: APT package indices\n'
    for path in "${apt_paths[@]}"; do
        expect_code "${RELEASE_BASE}/${path}/Packages.gz" "200" "release/${path}/Packages.gz"
    done

    printf 'Release channel: DNF repository files\n'
    for path in "${dnf_paths[@]}"; do
        expect_code "${RELEASE_BASE}/${path}/linglong%3ACI%3Arelease.repo" "200" "release/${path}/linglong:CI:release.repo"
    done

    # Ubuntu 25.x is published WITHOUT the OBS "x" prefix that Ubuntu 24.04
    # keeps. Guard both channels so a well-meant rename to "xUbuntu_25.x"
    # cannot silently reintroduce a 404.
    printf 'Regression guard: Ubuntu 25.x naming (both channels)\n'
    for base in "${RELEASE_BASE}" "${LATEST_BASE}"; do
        local channel
        channel="${base##*/}"
        expect_code "${base}/Ubuntu_25.04/" "200" "${channel}/Ubuntu_25.04/"
        expect_code "${base}/Ubuntu_25.10/" "200" "${channel}/Ubuntu_25.10/"
        expect_code "${base}/xUbuntu_25.04/" "404" "${channel}/xUbuntu_25.04/"
        expect_code "${base}/xUbuntu_25.10/" "404" "${channel}/xUbuntu_25.10/"
    done

    if [[ "${FAILURES}" -ne 0 ]]; then
        printf '\n%s check(s) failed.\n' "${FAILURES}"
        return 1
    fi

    printf '\nAll documented OBS repository paths are reachable.\n'
}

main "$@"
