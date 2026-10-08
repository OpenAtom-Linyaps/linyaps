#!/bin/bash
# SPDX-FileCopyrightText: 2026 Hanabi9249
#
# SPDX-License-Identifier: LGPL-3.0-or-later
# Standalone argv-boundary regression, not an ELF/ldd integration test.
# Usage: bash misc/tests/ldd-check-filenames_test.sh [helper] [scratch-parent]
# Set KEEP_FIXTURES=1 to retain argv captures and fixtures for inspection.
set -eo pipefail
scriptdir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
subject=$(realpath "${1:-$scriptdir/../libexec/linglong/builder/helper/ldd-check.sh}")
scratch=$(cd -- "${2:-${TMPDIR:-/tmp}}" && pwd -P)
work=$(mktemp -d "$scratch/ldd-filenames.XXXXXX")
readonly scratch work
cleanup() {
    local status=$?
    if [[ ${KEEP_FIXTURES:-0} == 1 ]]; then
        printf 'Retained fixtures and NUL-separated argv: %s\n' "$work"
        return "$status"
    fi
    # Delete only this invocation's exclusive mktemp directory, never its parent.
    # Resolve again before recursive removal; reject symlinks or changed ownership paths.
    if [[ $work != /* || $work != "${scratch%/}/ldd-filenames."* ||
        ! ${work##*/} =~ ^ldd-filenames\.[[:alnum:]]{6}$ || -L $work ||
        $(realpath -e -- "$work") != "$work" ]]; then
        printf 'Refusing cleanup outside owned temporary directory: %s\n' "$work" >&2
        return 1
    fi
    rm -rf -- "$work" || return 1
    printf 'Removed owned temporary directory: %s\n' "$work"
    return "$status"
}
trap cleanup EXIT
export TMPDIR="$work/tmp"
mkdir -p "$TMPDIR" "$work/shims"
export NM_CAPTURE LDD_CAPTURE NM_READABLE

# External executables capture real shell argv. They deliberately do not inspect ELF.
cat > "$work/shims/nm" <<'SHIM'
#!/bin/bash
printf '%s\0' "$#" "$@" >> "$NM_CAPTURE"
{ printf 'argc=%s' "$#"; printf ' <%q>' "$@"; printf '\n'; } >> "$NM_READABLE"
if [[ $# == 3 && $1 == -D && -f $2 && $3 == /dev/null ]]; then
    printf '                 U __libc_start_main\n'
else
    exit 1
fi
SHIM
cat > "$work/shims/ldd" <<'SHIM'
#!/bin/bash
printf '%s\0' "$#" "$@" >> "$LDD_CAPTURE"
[[ $# == 1 && -f $1 ]] || exit 1
printf 'linux-vdso.so.1 (0x00000000)\n'
SHIM
chmod +x "$work/shims/nm" "$work/shims/ldd"
export PATH="$work/shims:$PATH"

failed=0
for scenario in plain single-space repeated-space; do
    root="$work/$scenario/files"
    mkdir -p "$root"
    case "$scenario" in
        plain) touch "$root/one" "$root/two" "$root/three" ;;
        single-space) touch "$root/only program" ;;
        repeated-space) touch "$root/one program" "$root/two program" "$root/three program" ;;
    esac
    NM_CAPTURE="$work/$scenario/nm.actual.nul"
    NM_READABLE="$work/$scenario/nm.actual.txt"
    LDD_CAPTURE="$work/$scenario/ldd.actual.nul"
    : > "$NM_CAPTURE"
    : > "$NM_READABLE"
    : > "$LDD_CAPTURE"
    : > "$work/$scenario/nm.expected.nul"
    : > "$work/$scenario/ldd.expected.nul"
    # Use real find, and record its order instead of assuming a sorted traversal.
    find "$root" -type f > "$work/$scenario/find.txt"
    while IFS= read -r file; do
        printf '%s\0' 3 -D "$file" /dev/null >> "$work/$scenario/nm.expected.nul"
        printf '%s\0' 1 "$file" >> "$work/$scenario/ldd.expected.nul"
    done < "$work/$scenario/find.txt"

    # Source the actual file unchanged. Empty argv takes its harmless usage branch.
    # The separate Bash process resets IFS/readonly declarations for every scenario.
    # Invoke collectDependsLibs directly to avoid main's fixed /project output path.
    status=0
    bash -c 'source "$1" "" >/dev/null; collectDependsLibs "$2"' \
        bash "$subject" "$root" > "$work/$scenario/collector.log" 2>&1 || status=$?
    nm_ok=no
    ldd_ok=no
    cmp -s "$NM_CAPTURE" "$work/$scenario/nm.expected.nul" && nm_ok=yes
    cmp -s "$LDD_CAPTURE" "$work/$scenario/ldd.expected.nul" && ldd_ok=yes
    if [[ $status == 0 && $nm_ok == yes && $ldd_ok == yes ]]; then
        printf 'PASS %s: collector=%s nm-argv=%s ldd-visits=%s\n' "$scenario" "$status" "$nm_ok" "$ldd_ok"
    else
        printf 'FAIL %s: collector=%s nm-argv=%s ldd-visits=%s\n' "$scenario" "$status" "$nm_ok" "$ldd_ok"
        failed=$((failed + 1))
    fi
    cat "$NM_READABLE"
done
[[ $failed == 0 ]]
