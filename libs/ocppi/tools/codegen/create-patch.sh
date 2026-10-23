#!/bin/bash

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"/tools/codegen || exit 255

include="$repoRoot/include"
origin="$include.orig"

[ -f "$origin" ] && chmod -R u+w "$origin"
rm -rf "$origin"

cp -r "$include" "$origin"

chmod -R u-w "$origin"

# Resolve an interactive shell in the order the environment offers one. The
# passwd lookup alone is not enough: in a container or a CI job the login is
# frequently absent from /etc/passwd, the substitution then yields an empty
# string, and "$USER_SHELL" below fails with ": command not found" -- leaving a
# read-only include.orig behind because the diff never runs.
resolve_user_shell() {
    local candidate

    candidate=${USER_SHELL:-}
    if [ -z "$candidate" ] && command -v getent >/dev/null 2>&1; then
        candidate=$(getent passwd | awk -F: -v user="${USER:-}" '$1 == user {print $NF; exit}')
    fi
    if [ -z "$candidate" ]; then
        candidate=${SHELL:-}
    fi

    # Only accept something that is actually runnable.
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
        printf '%s\n' "$candidate"
        return 0
    fi

    for candidate in /bin/bash /bin/sh; do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

if ! USER_SHELL=$(resolve_user_shell); then
    echo "cannot find a usable shell; set USER_SHELL to your interactive shell" >&2
    exit 255
fi

echo "Make your change in include directory then exit shell"

cd "$repoRoot"

"$USER_SHELL"

diff -ruN "${origin#"$repoRoot/"}" "${include#"$repoRoot/"}" \
	>"$repoRoot"/tools/codegen/fix.patch &&
	exit 1

chmod -R u+w "$origin"
rm -rf "$origin"
