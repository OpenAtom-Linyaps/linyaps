#!/bin/sh
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
set -eu

fetcher=$1
scenario=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/cache" "$work/bin" "$work/barrier"
printf 'source contents\n' > "$work/expected"
digest=$(sha256sum "$work/expected" | awk '{print $1}')
export FETCH_TEST_WORK="$work"
export PATH="$work/bin:$PATH"
cat > "$work/bin/wget" << 'WGET'
#!/bin/sh
set -eu
case "$FETCH_TEST_SCENARIO" in
    concurrent)
        cp "$FETCH_TEST_WORK/expected" "$3"
        touch "$FETCH_TEST_WORK/barrier/ready.$$"
        attempts=0
        while [ "$(find "$FETCH_TEST_WORK/barrier" -type f | wc -l)" -lt 2 ]; do
            attempts=$((attempts + 1))
            [ "$attempts" -lt 500 ] || exit 1
            sleep 0.01
        done
        ;;
    download_failure)
        printf 'partial' > "$3"
        exit 1
        ;;
    digest_mismatch)
        printf 'wrong contents' > "$3"
        ;;
    cache_hit)
        exit 1
        ;;
esac
WGET
chmod +x "$work/bin/wget"
export FETCH_TEST_SCENARIO="$scenario"

case "$scenario" in
    concurrent)
        sh "$fetcher" "$work/first" ignored "$digest" "$work/cache" > "$work/first.log" 2>&1 &
        first=$!
        sh "$fetcher" "$work/second" ignored "$digest" "$work/cache" > "$work/second.log" 2>&1 &
        second=$!
        status=0
        wait "$first" || status=1
        wait "$second" || status=1
        if [ "$status" -ne 0 ]; then
            cat "$work/first.log" "$work/second.log"
            exit 1
        fi
        cmp "$work/expected" "$work/first"
        cmp "$work/expected" "$work/second"
        cmp "$work/expected" "$work/cache/file_$digest"
        test "$(stat -c %a "$work/first")" = "$(stat -c %a "$work/expected")"
        ;;
    download_failure | digest_mismatch)
        if sh "$fetcher" "$work/output" ignored "$digest" "$work/cache"; then
            echo 'Unexpected successful download'
            exit 1
        fi
        test ! -e "$work/output"
        test ! -e "$work/cache/file_$digest"
        ;;
    cache_hit)
        cp "$work/expected" "$work/cache/file_$digest"
        sh "$fetcher" "$work/output" ignored "$digest" "$work/cache"
        cmp "$work/expected" "$work/output"
        ;;
    *) exit 1 ;;
esac

test -z "$(find "$work/cache" -name 'tmp_*' -print)"
