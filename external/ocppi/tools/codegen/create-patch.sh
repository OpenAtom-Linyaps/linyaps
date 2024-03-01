#!/bin/bash

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

USER_SHELL=${USER_SHELL:="$(getent passwd | awk -F: -v user="$USER" '$1 == user {print $NF}')"}

echo "Make your change in include directory then exit shell"

cd "$repoRoot"

"$USER_SHELL"

diff -ruN "${origin#"$repoRoot/"}" "${include#"$repoRoot/"}" \
	>"$repoRoot"/tools/codegen/fix.patch &&
	exit 1

chmod -R u+w "$origin"
rm -rf "$origin"
