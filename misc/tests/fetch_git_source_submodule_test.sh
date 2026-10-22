#!/bin/sh

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Calls misc/libexec/linglong/fetch-git-source. Uses only local file:// repositories.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
helper=$(CDPATH= cd -- "$script_dir/../libexec/linglong" && pwd)/fetch-git-source
tmpdir=$(mktemp -d)
cache=$tmpdir/cache
trap 'rm -rf "$tmpdir"' EXIT

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

[ -f "$helper" ] || fail "helper not found: $helper"

export GIT_CONFIG_NOSYSTEM=1
export GIT_CONFIG_GLOBAL=/dev/null
export GIT_CONFIG_COUNT=2
export GIT_CONFIG_KEY_0=protocol.file.allow
export GIT_CONFIG_VALUE_0=always
export GIT_CONFIG_KEY_1=init.defaultBranch
export GIT_CONFIG_VALUE_1=main
export GIT_AUTHOR_NAME=Fixture
export GIT_AUTHOR_EMAIL=fixture@example.invalid
export GIT_COMMITTER_NAME=Fixture
export GIT_COMMITTER_EMAIL=fixture@example.invalid

file_uri() {
    case $1 in
        /*) ;;
        *) fail "absolute path required: $1" ;;
    esac
    printf 'file://%s\n' "$1"
}

init_repo() {
    mkdir -p "$1"
    git -C "$1" init -q -b main
}

commit_repo() {
    git -C "$1" add .
    git -C "$1" commit -qm "$2"
    git -C "$1" rev-parse HEAD
}

run_helper() {
    GIT_SUBMODULES=$4 sh "$helper" "$1" "$2" "$3" "$cache" >"$5" 2>"$6"
}

require_helper() {
    if ! run_helper "$1" "$2" "$3" "$4" "$5" "$6"; then
        cat "$6" >&2
        fail "$7"
    fi
}

# Same submodule URL, later commit. The reused workdir must move forward.
same=$tmpdir/same
super_same=$tmpdir/super-same
work_same=$tmpdir/work-same
init_repo "$same"
printf 'a\n' >"$same/payload"
same_a=$(commit_repo "$same" a)
init_repo "$super_same"
git -C "$super_same" submodule add "$(file_uri "$same")" deps/library >/dev/null 2>&1
pin_a=$(commit_repo "$super_same" "pin a")
require_helper "$work_same" "$(file_uri "$super_same")" "$pin_a" true \
    "$tmpdir/same-a.out" "$tmpdir/same-a.err" "same-url first fetch failed"
got=$(git -C "$work_same/deps/library" rev-parse HEAD)
[ "$got" = "$same_a" ] || fail "same-url first submodule is $got, want $same_a"
printf 'b\n' >"$same/payload"
same_b=$(commit_repo "$same" b)
git -C "$super_same/deps/library" fetch "$(file_uri "$same")" "$same_b" >/dev/null 2>&1
git -C "$super_same/deps/library" checkout -q --detach FETCH_HEAD
git -C "$super_same" add deps/library
pin_b=$(commit_repo "$super_same" "pin b")
require_helper "$work_same" "$(file_uri "$super_same")" "$pin_b" true \
    "$tmpdir/same-b.out" "$tmpdir/same-b.err" "same-url second fetch failed"
got=$(git -C "$work_same/deps/library" rev-parse HEAD)
[ "$got" = "$same_b" ] || fail "same-url second submodule is $got, want $same_b"
[ "$(git -C "$work_same/deps/library" rev-parse --is-shallow-repository)" = true ] ||
    fail "same-url fetch did not keep depth 1"
printf 'PASS same-url commit advance %s\n' "$got"

# Repository without submodules, whether or not submodule fetching is enabled.
plain=$tmpdir/plain
work_plain=$tmpdir/work-plain
init_repo "$plain"
printf 'plain\n' >"$plain/payload"
plain_sha=$(commit_repo "$plain" plain)
require_helper "$work_plain" "$(file_uri "$plain")" "$plain_sha" true \
    "$tmpdir/plain-on.out" "$tmpdir/plain-on.err" "non-submodule fetch with GIT_SUBMODULES failed"
got=$(git -C "$work_plain" rev-parse HEAD)
[ "$got" = "$plain_sha" ] || fail "non-submodule HEAD is $got, want $plain_sha"
require_helper "$work_plain" "$(file_uri "$plain")" "$plain_sha" "" \
    "$tmpdir/plain-off.out" "$tmpdir/plain-off.err" "non-submodule fetch without GIT_SUBMODULES failed"
got=$(git -C "$work_plain" rev-parse HEAD)
[ "$got" = "$plain_sha" ] || fail "repeated non-submodule HEAD is $got, want $plain_sha"
printf 'PASS non-submodule repository %s\n' "$got"

# An unreachable recorded submodule commit must still fail the helper.
old=$tmpdir/old
bad=$tmpdir/bad
work_bad=$tmpdir/work-bad
init_repo "$old"
printf 'old\n' >"$old/payload"
commit_repo "$old" old >/dev/null
init_repo "$bad"
git -C "$bad" submodule add "$(file_uri "$old")" deps/library >/dev/null 2>&1
git -C "$bad" update-index --cacheinfo 160000,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa,deps/library
git -C "$bad" commit -qm "bad gitlink"
bad_sha=$(git -C "$bad" rev-parse HEAD)
recorded=$(git -C "$bad" rev-parse HEAD:deps/library)
[ "$recorded" = aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ] ||
    fail "fixture gitlink is $recorded"
if run_helper "$work_bad" "$(file_uri "$bad")" "$bad_sha" true \
    "$tmpdir/bad.out" "$tmpdir/bad.err"; then
    fail "missing submodule commit exited 0"
fi
printf 'PASS missing submodule commit fails\n'

# Reused workdir whose .gitmodules URL now points at another repository.
new=$tmpdir/new
super=$tmpdir/super
work=$tmpdir/work
init_repo "$new"
printf 'new\n' >"$new/payload"
new_sha=$(commit_repo "$new" new)
init_repo "$super"
git -C "$super" submodule add "$(file_uri "$old")" deps/library >/dev/null 2>&1
first=$(commit_repo "$super" "old submodule")
require_helper "$work" "$(file_uri "$super")" "$first" true \
    "$tmpdir/initial.out" "$tmpdir/initial.err" "initial submodule fetch failed"
old_sha=$(git -C "$old" rev-parse HEAD)
got=$(git -C "$work/deps/library" rev-parse HEAD)
[ "$got" = "$old_sha" ] || fail "initial submodule is $got, want $old_sha"
git -C "$super" config -f .gitmodules submodule.deps/library.url "$(file_uri "$new")"
git -C "$super/deps/library" fetch "$(file_uri "$new")" "$new_sha" >/dev/null 2>&1
git -C "$super/deps/library" checkout -q --detach FETCH_HEAD
git -C "$super" add .gitmodules deps/library
second=$(commit_repo "$super" "move submodule to new remote")
recorded=$(git -C "$super" rev-parse HEAD:deps/library)
[ "$recorded" = "$new_sha" ] || fail "fixture gitlink is $recorded, want $new_sha"
if ! run_helper "$work" "$(file_uri "$super")" "$second" true \
    "$tmpdir/changed.out" "$tmpdir/changed.err"; then
    cat "$tmpdir/changed.err" >&2
    fail "changed submodule URL fetch failed"
fi
got=$(git -C "$work/deps/library" rev-parse HEAD)
[ "$got" = "$new_sha" ] || fail "changed-url submodule is $got, want $new_sha"
payload=$(cat "$work/deps/library/payload")
[ "$payload" = new ] || fail "changed-url payload is '$payload', want new"
configured=$(git -C "$work" config --get submodule.deps/library.url)
[ "$configured" = "$(file_uri "$new")" ] || fail "configured submodule URL is $configured"
origin=$(git -C "$work/deps/library" remote get-url origin)
[ "$origin" = "$(file_uri "$new")" ] || fail "submodule origin is $origin"
[ "$(git -C "$work/deps/library" rev-parse --is-shallow-repository)" = true ] ||
    fail "changed-url fetch did not keep depth 1"
printf 'PASS changed submodule URL %s\n' "$got"

# Empty GIT_SUBMODULES leaves the submodule unchecked out.
work_off=$tmpdir/work-off
require_helper "$work_off" "$(file_uri "$super")" "$second" "" \
    "$tmpdir/off.out" "$tmpdir/off.err" "fetch with submodules disabled failed"
[ "$(git -C "$work_off" rev-parse HEAD)" = "$second" ] || fail "disabled fetch checked out the wrong commit"
[ ! -e "$work_off/deps/library/.git" ] || fail "disabled fetch still checked out the submodule"
printf 'PASS submodules disabled\n'
