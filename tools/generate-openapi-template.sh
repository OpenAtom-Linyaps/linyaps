#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot/tools"

cleanup() {
        cd "$(git rev-parse --show-toplevel)"
        cd tools
        rm openapi-generator -rf
}
trap cleanup EXIT

cd "$(git rev-parse --show-toplevel)"/tools

rm ./openapi-cpp-qt-client/*.mustache -f
git clone --depth 1 https://github.com/OpenAPITools/openapi-generator -b v7.1.0
mkdir -p openapi-cpp-qt-client
cp ./openapi-generator/modules/openapi-generator/src/main/resources/cpp-qt-client/*.mustache ./openapi-cpp-qt-client/

patch -s -p0 <./no-qt-keywords.patch
patch -s -p0 <./no-cmake.patch
