#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -euo pipefail

command -v eu-strip > /dev/null || {
    echo 'symbols-strip test requires eu-strip from elfutils' >&2
    exit 1
}

helper="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../libexec/linglong/builder/helper/symbols-strip.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/temps" "$tmp/no-id" "$tmp/with-id" "$tmp/failure" "$tmp/bin"
export TMPDIR="$tmp/temps"
printf 'int main(void) { return 0; }\n' > "$tmp/main.c"
${CC:-cc} -g -Wl,--build-id=none "$tmp/main.c" -o "$tmp/no-id/app"
before=$(sha256sum "$tmp/no-id/app")
PREFIX="$tmp/no-id" bash "$helper"
[[ $(sha256sum "$tmp/no-id/app") == "$before" ]]
[[ ! -e "$tmp/no-id/lib/debug/.build-id" ]]
[[ -z $(ls -A "$TMPDIR") ]]

${CC:-cc} -g -Wl,--build-id=sha1 "$tmp/main.c" -o "$tmp/original"
cp "$tmp/original" "$tmp/with-id/app"
for run in 1 2; do
    PREFIX="$tmp/with-id" bash "$helper"
    file "$tmp/with-id/app" | grep -qv 'not stripped'
    [[ $(find "$tmp/with-id/lib/debug/.build-id" -type f -name '*.debug' | wc -l) -eq 1 ]]
    [[ -z $(find "$tmp/with-id/lib/debug" -xtype l) ]]
    [[ -z $(ls -A "$TMPDIR") ]]
    if [[ $run -eq 1 ]]; then
        cp "$tmp/original" "$tmp/with-id/app"
    fi
done

cp "$tmp/original" "$tmp/failure/app"
printf '#!/bin/sh\necho "injected eu-strip failure" >&2\nexit 17\n' > "$tmp/bin/eu-strip"
chmod +x "$tmp/bin/eu-strip"
if output=$(PATH="$tmp/bin:$PATH" PREFIX="$tmp/failure" bash "$helper" 2>&1); then
    echo 'Expected eu-strip failure to propagate' >&2
    exit 1
else
    status=$?
fi
[[ $status -eq 17 && $output == *'injected eu-strip failure'* ]]
[[ -z $(ls -A "$TMPDIR") ]]

printf '#!/bin/sh\necho "injected readelf failure" >&2\nexit 23\n' > "$tmp/bin/readelf"
chmod +x "$tmp/bin/readelf"
if output=$(PATH="$tmp/bin:$PATH" PREFIX="$tmp/failure" bash "$helper" 2>&1); then
    echo 'Expected readelf failure to propagate' >&2
    exit 1
else
    status=$?
fi
[[ $status -eq 23 && $output == *'injected readelf failure'* ]]
[[ -z $(ls -A "$TMPDIR") ]]

echo 'symbols-strip: no Build ID, repeated stripping, strip failure and readelf failure passed'
