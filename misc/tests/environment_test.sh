#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eu

test_dir="$1"
repo_dir="$2"
bin_dir="${repo_dir}/entries/bin"
baseline="/usr/bin:/bin"
expected_xdg="/usr/local/share:/usr/share:${repo_dir}/entries/share"

assert_line() {
    case "
$1
" in
        *"
$2
"*) ;;
        *)
            printf 'Missing expected line: %s\nActual output:\n%s\n' "$2" "$1" >&2
            exit 1
            ;;
    esac
}

# Both system and user managers install the same generator. It must work even
# before the repository is mounted, preserve system command priority, and avoid
# adding an entry that is already at the beginning, middle, or end of PATH.
if [ -d "${repo_dir}" ]; then
    printf 'Test requires a missing repository: %s\n' "${repo_dir}" >&2
    exit 1
fi
assert_generator() {
    output=$(env PATH="$1" XDG_DATA_DIRS= /bin/sh "${test_dir}/61-linglong")
    assert_line "${output}" "PATH=$2"
    assert_line "${output}" "XDG_DATA_DIRS=${expected_xdg}"
}
assert_generator "${baseline}" "${baseline}:${bin_dir}"
assert_generator "${bin_dir}:${baseline}" "${bin_dir}:${baseline}"
assert_generator "/usr/bin:${bin_dir}:/bin" "/usr/bin:${bin_dir}:/bin"
assert_generator "${baseline}:${bin_dir}" "${baseline}:${bin_dir}"
assert_generator "" "${bin_dir}"

# A login can reset PATH before sourcing profile.d and then source the same
# script again via Xsession.d. Both variables must be exported to child programs,
# and neither sourcing pass should print anything or duplicate directory entries.
output=$(env XDG_DATA_DIRS= /bin/sh -c '
    unset PATH XDG_DATA_DIRS
    PATH=/usr/bin:/bin
    . "$1"
    . "$1"
    /usr/bin/env
' sh "${test_dir}/linglong.sh")
assert_line "${output}" "PATH=${baseline}:${bin_dir}"
assert_line "${output}" "XDG_DATA_DIRS=${expected_xdg}"

# Keep existing application data directories when extending the environment.
output=$(env PATH="${baseline}" XDG_DATA_DIRS=/custom/share /bin/sh "${test_dir}/61-linglong")
assert_line "${output}" "XDG_DATA_DIRS=/custom/share:${repo_dir}/entries/share"

printf 'Environment script tests passed\n'
