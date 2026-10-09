#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Wang1rrr
# SPDX-License-Identifier: LGPL-3.0-or-later

set -euo pipefail

export_script="$1"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

mock_bin="$tmp_dir/mock bin"
mkdir -p "$mock_bin"
cat > "$mock_bin/mkfs.erofs" <<'MOCK'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$MKFS_ARGS_FILE"
MOCK
chmod +x "$mock_bin/mkfs.erofs"

export PATH="$mock_bin:$PATH"
export MKFS_ARGS_FILE="$tmp_dir/mkfs-args"

pack_dir="$tmp_dir/pack dir:with colon"
output_file="$tmp_dir/output dir:with colon/image.erofs"
mkdir -p "$pack_dir"

"$export_script" --packdir "$pack_dir" --output "$output_file" -z 'lz4hc,9'
printf '%s\n' \
    -z 'lz4hc,9' \
    -Efragments,dedupe,ztailpacking \
    -C1048576 \
    -b4096 \
    "$output_file" \
    "$pack_dir" > "$tmp_dir/expected"
diff -u "$tmp_dir/expected" "$MKFS_ARGS_FILE"

# Preserve the old colon-delimited form for callers that have not migrated yet.
legacy_output="$tmp_dir/legacy output/image.erofs"
legacy_pack_dir="$tmp_dir/legacy pack dir"
mkdir -p "$legacy_pack_dir"
"$export_script" --packdir "$legacy_pack_dir:$legacy_output"
printf '%s\n' \
    -Efragments,dedupe,ztailpacking \
    -C1048576 \
    -b4096 \
    "$legacy_output" \
    "$legacy_pack_dir" > "$tmp_dir/expected"
diff -u "$tmp_dir/expected" "$MKFS_ARGS_FILE"

if "$export_script" --packdir "$pack_dir" > /dev/null 2>&1; then
    echo "expected --packdir without --output to fail" >&2
    exit 1
fi
