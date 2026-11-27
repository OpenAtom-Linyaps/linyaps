#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -euo pipefail

helper="$(dirname "${BASH_SOURCE[0]}")/../libexec/linglong/builder/helper/config-check.sh"

assert_failure() {
    local expected="$1" output status
    shift
    if output=$("$@" bash "$helper" 2>&1); then
        echo "Expected config-check to fail: $expected" >&2
        exit 1
    else
        status=$?
    fi
    if [[ $status -ne 255 || $output != "$expected" ]]; then
        printf 'Expected status 255 and: %s\nActual status %s and: %s\n' \
            "$expected" "$status" "$output" >&2
        exit 1
    fi
}

assert_failure 'LINGLONG_APPID is not set.' env -u LINGLONG_APPID
assert_failure 'LINGLONG_APPID is not set.' env LINGLONG_APPID=
assert_failure '/opt/apps/org.example.linyaps.config-check-test-missing does not exist.' \
    env LINGLONG_APPID=org.example.linyaps.config-check-test-missing

echo 'config-check: 3 cases passed'
