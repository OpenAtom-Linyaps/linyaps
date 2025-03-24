#!/bin/bash

# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -e

# 定义 CMakeLists.txt 文件路径
CMAKE_FILE="CMakeLists.txt"

# 检查文件是否存在
if [[ ! -f "$CMAKE_FILE" ]]; then
        echo "错误: $CMAKE_FILE 文件不存在!"
        exit 1
fi

# 提取当前的 VERSION 字段
CURRENT_VERSION=$(tr -d '\n' < "$CMAKE_FILE" | grep -oP 'project\(\s*\w+\s*VERSION\s*\K[0-9.]+')

# 检查是否找到 VERSION
if [[ -z "$CURRENT_VERSION" ]]; then
        echo "错误: 未找到 VERSION 字段!"
        exit 1
fi

echo "当前的 VERSION: $CURRENT_VERSION"

# 提示输入新版本号
read -p "请输入新版本号 (留空则自动递增小版本号): " NEW_VERSION

# 如果未输入新版本号，则自动递增小版本号
if [[ -z "$NEW_VERSION" ]]; then
        IFS='.' read -r -a VERSION_PARTS <<<"$CURRENT_VERSION"
        LAST_PART=${VERSION_PARTS[${#VERSION_PARTS[@]} - 1]}
        NEW_LAST_PART=$((LAST_PART + 1))
        VERSION_PARTS[${#VERSION_PARTS[@]} - 1]=$NEW_LAST_PART
        NEW_VERSION=$(
                IFS='.'
                echo "${VERSION_PARTS[*]}"
        )
        echo "自动递增小版本号: $NEW_VERSION"
fi

# 更新 CMakeLists.txt 文件中的 VERSION 字段
sed -E -i "/project\s*\(/,/\)/{
    s/(VERSION\s+)[0-9]+\.[0-9]+\.[0-9]+/\1${NEW_VERSION}/
}" "$CMAKE_FILE"

# 检查是否更新成功
UPDATED_VERSION=$(tr -d '\n' < "$CMAKE_FILE" | grep -oP 'project\(\s*\w+\s*VERSION\s*\K[0-9.]+')
if [[ "$UPDATED_VERSION" != "$NEW_VERSION" ]]; then
        echo "错误: 更新 VERSION 字段失败!"
        exit 1
fi

echo "更新成功! 新的 VERSION: $UPDATED_VERSION"

# 提交更改到 Git
git add "$CMAKE_FILE"
git commit -m "release $UPDATED_VERSION"

# 打 Git 标签
TAG_NAME="$UPDATED_VERSION"
git tag "$TAG_NAME" -m "release $UPDATED_VERSION"
