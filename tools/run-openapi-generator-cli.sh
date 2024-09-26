#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot/tools"

output=../external/http2
# 单个接口可能存在有多个tag，openapi-generator-cli会为每个tag生成client文件，
# 这些client之间有重复并且在客户端不会被用到，所以这里只保留第一个tag
onlyFirstTag=KEEP_ONLY_FIRST_TAG_IN_OPERATION=true
# 服务端使用自定义的@X-Role生成接口权限文档，openapi-generator-cli无法识别，所以跳过验证
skipValidate=--skip-validate-spec
# 清理输出目录
rm -r $output || true

openapi-generator-cli() {
        if [ -z "$OPENAPI_GENERATOR_CLI" ]; then
                npm install @openapitools/openapi-generator-cli
                npx openapi-generator-cli "$@"
        else
                $OPENAPI_GENERATOR_CLI "$@"
        fi
}

openapi-generator-cli generate -g c -o "$output" \
        -i ../api/http/client_swagger.json \
        $skipValidate \
        --openapi-normalizer $onlyFirstTag \
        --template-dir "$repoRoot/tools/openapi-c-libcurl-client"

rm -r $output/docs
