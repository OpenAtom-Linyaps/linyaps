#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Regression test: ldd-check.sh must pass each ':'-separated path to find as a
# separate argument and must not treat empty fields as the current directory.

set -euo pipefail

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

script_dir=$(cd "$(dirname "$0")" && pwd)
helper="$script_dir/../libexec/linglong/builder/helper/ldd-check.sh"
[[ -f $helper ]] || fail "helper not found: $helper"

bash -n "$helper" || fail "bash -n reported syntax errors"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/a" "$tmp/b" "$tmp/bin"
touch "$tmp/a/file-a" "$tmp/b/file-b"

# PATH-shadowed find: record argv as NUL-separated fields, then exit 0 so the
# helper can continue (it may later fail writing /project/linglong/depends.yaml,
# which is outside this test's scope).
cat > "$tmp/bin/find" << 'EOF'
#!/bin/bash
printf '%s\0' "$@" >"${FIND_ARGS_FILE:?}"
exit 0
EOF
chmod +x "$tmp/bin/find"

export FIND_ARGS_FILE="$tmp/find-args"

run_helper() {
    # The helper may exit non-zero on the hardcoded /project write; the
    # regression assertion is the argv recorded by the stubbed find.
    PATH="$tmp/bin:$PATH" bash "$helper" "$1" > /dev/null 2>&1 || true
}

assert_find_args() {
    local label=$1
    shift
    mapfile -d '' -t args < "$FIND_ARGS_FILE"
    local expected=("$@")
    local i
    if [[ ${#args[@]} -ne ${#expected[@]} ]]; then
        fail "$label: expected ${#expected[@]} args, got ${#args[@]}: ${args[*]}"
    fi
    for i in "${!expected[@]}"; do
        if [[ ${args[$i]} != "${expected[$i]}" ]]; then
            fail "$label: arg[$i] expected '${expected[$i]}', got '${args[$i]}'"
        fi
    done
    echo "PASS: $label"
}

# Two directories must arrive as two separate find arguments.
rm -f "$FIND_ARGS_FILE"
run_helper "$tmp/a:$tmp/b"
[[ -f $FIND_ARGS_FILE ]] || fail "find was not invoked"
assert_find_args "two paths" "$tmp/a" "$tmp/b" -type f

# Empty fields from leading/trailing/adjacent colons must be dropped.
rm -f "$FIND_ARGS_FILE"
run_helper "$tmp/a::$tmp/b:"
assert_find_args "empty fields dropped" "$tmp/a" "$tmp/b" -type f

# A single path still works.
rm -f "$FIND_ARGS_FILE"
run_helper "$tmp/a"
assert_find_args "single path" "$tmp/a" -type f

# An input made only of colons must be rejected as "No paths provided".
if PATH="$tmp/bin:$PATH" bash "$helper" ":::" > /dev/null 2>&1; then
    fail "colon-only input should be rejected"
fi
echo "PASS: colon-only input rejected"

echo "All ldd-check path tests passed."
