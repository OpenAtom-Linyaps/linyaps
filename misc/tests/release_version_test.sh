#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eu

source_dir=$1
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT

cp "${source_dir}/release.bash" "${test_dir}/release.bash"
cat >"${test_dir}/CMakeLists.txt" <<'EOF'
project(
  linglong
  VERSION 1.15.0)
EOF

cd "${test_dir}"
git init --quiet
git config user.name "Linyaps Test"
git config user.email "test@linyaps.invalid"
git add CMakeLists.txt release.bash
git commit --quiet -m "initial"

head_before=$(git rev-parse HEAD)
tags_before=$(git tag)

if printf '1.2.invalid\n' | bash ./release.bash; then
    printf 'release.bash accepted an invalid version\n' >&2
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    printf 'release.bash changed the worktree for an invalid version:\n' >&2
    git status --short >&2
    exit 1
fi

if [ "$(git rev-parse HEAD)" != "${head_before}" ]; then
    printf 'release.bash created a commit for an invalid version\n' >&2
    exit 1
fi

if [ "$(git tag)" != "${tags_before}" ]; then
    printf 'release.bash created a tag for an invalid version\n' >&2
    exit 1
fi

printf 'Release version validation test passed\n'
