#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 PointerL
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e

completion_file="$1"
command="$2"

# Keep repository queries deterministic and avoid changing the user's repository.
repo_show() {
    [[ "$*" == "repo show" ]] || return 1
    printf '%s\n' \
        'Default: stable' \
        'Name Url Alias Priority' \
        'upstream https://example.invalid stable 0' \
        'another https://example.invalid update 0'
}
ll-cli() { repo_show "$@"; }
ll-builder() { repo_show "$@"; }

source "$completion_file"
case "$command" in
ll-cli) completion_function=_ll_cli_completion ;;
ll-builder) completion_function=_ll_builder_complete ;;
*) exit 1 ;;
esac

failures=0
check_completion() {
    local expected="$1"
    shift
    COMP_WORDS=("$command" "$@")
    COMP_CWORD=$(("${#COMP_WORDS[@]}" - 1))
    if ! "$completion_function"; then
        printf 'Completion function failed: %s\n' "${COMP_WORDS[*]}" >&2
        exit 1
    fi
    if [[ "${COMPREPLY[*]}" != "$expected" ]]; then
        printf '%s: expected <%s>, got <%s>\n' \
            "${COMP_WORDS[*]}" "$expected" "${COMPREPLY[*]}" >&2
        failures=$((failures + 1))
    fi
}

check_completion stable repo enable-mirror sta
check_completion update repo enable-mirror upd
check_completion --region repo enable-mirror --r
check_completion "--help --help-all" repo enable-mirror --h
check_completion --region repo enable-mirror stable --r
check_completion --region repo enable-mirror update --r
check_completion "" repo enable-mirror stable --region cn --r
check_completion "" repo enable-mirror missing
check_completion stable repo disable-mirror sta
check_completion stable repo remove sta
check_completion stable repo update sta
check_completion stable repo set-default sta
check_completion stable repo set-priority sta
check_completion enable-mirror repo en

if [[ "$failures" -ne 0 ]]; then
    exit 1
fi
printf '%s repository completion tests passed\n' "$command"
