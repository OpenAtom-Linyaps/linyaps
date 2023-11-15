#!/bin/env bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -x
set -e

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot/tools"

output=../external/http
cppNamespace=linglong::api::client
onlyFirstTag=KEEP_ONLY_FIRST_TAG_IN_OPERATION=true

openapi-generator-cli() {
        if [ -z "$OPENAPI_GENERATOR_CLI" ]; then
                npm install @openapitools/openapi-generator-cli
                exec npx openapi-generator-cli "$@"
        else
                exec $OPENAPI_GENERATOR_CLI "$@"
        fi
}

openapi-generator-cli generate -g cpp-qt-client -o "$output" \
        -i ../api/http/client_swagger.json \
        --openapi-normalizer $onlyFirstTag \
        --additional-properties="modelNamePrefix=,optionalProjectFile=false,cppNamespace=$cppNamespace" \
        --package-name=QtLinglongRepoClientAPI \
        --template-dir "$repoRoot/tools/openapi-cpp-qt-client"
