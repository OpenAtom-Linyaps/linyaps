#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -euo pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
output="$repoRoot/libs/api/http"
readonly output

if [[ ! -f "$repoRoot/CMakeLists.txt" || ! -d "$repoRoot/libs/api" ]]; then
        echo "refusing to generate HTTP client outside the linyaps source tree" >&2
        exit 1
fi

cd "$repoRoot/tools"
# 单个接口可能存在有多个tag，openapi-generator-cli会为每个tag生成client文件，
# 这些client之间有重复并且在客户端不会被用到，所以这里只保留第一个tag
onlyFirstTag="KEEP_ONLY_FIRST_TAG_IN_OPERATION=true"
# 服务端使用自定义的@X-Role生成接口权限文档，openapi-generator-cli无法识别，所以跳过验证
skipValidate="--skip-validate-spec"
# 清理输出目录
rm -rf -- "$output"

openapi-generator-cli() {
        if [ -z "${OPENAPI_GENERATOR_CLI:-}" ]; then
                npm install @openapitools/openapi-generator-cli
                npx openapi-generator-cli "$@"
        else
                "$OPENAPI_GENERATOR_CLI" "$@"
        fi
}

openapi-generator-cli generate -g c -o "$output" \
        -i ../api/http/client_swagger.json \
        "$skipValidate" \
        --openapi-normalizer "$onlyFirstTag" \
        --template-dir "$repoRoot/tools/openapi-c-libcurl-client"

rm -rf -- \
        "$output/docs" \
        "$output/unit-test" \
        "$output/README.md" \
        "$output/uncrustify-rules.cfg" \
        "$output/Packing.cmake" \
        "$output/.openapi-generator" \
        "$output/.openapi-generator-ignore"
