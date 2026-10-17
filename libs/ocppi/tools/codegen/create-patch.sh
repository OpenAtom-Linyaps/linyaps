#!/bin/bash

set -e
set -o pipefail

# ocppi 可能被作为子目录嵌入其他仓库（如 linyaps 的 libs/ocppi），
# 此时 git 顶层目录并不是 ocppi 根目录。以脚本自身位置定位 ocppi 根目录。
ocppiRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ocppiRoot"/tools/codegen || exit 255

include="$ocppiRoot/include"
origin="$include.orig"

[ -f "$origin" ] && chmod -R u+w "$origin"
rm -rf "$origin"

cp -r "$include" "$origin"

chmod -R u-w "$origin"

USER_SHELL=${USER_SHELL:="$(getent passwd | awk -F: -v user="$USER" '$1 == user {print $NF}')"}

echo "Make your change in include directory then exit shell"

cd "$ocppiRoot"

"$USER_SHELL"

diff -ruN "${origin#"$ocppiRoot/"}" "${include#"$ocppiRoot/"}" \
	>"$ocppiRoot"/tools/codegen/fix.patch &&
	exit 1

chmod -R u+w "$origin"
rm -rf "$origin"
