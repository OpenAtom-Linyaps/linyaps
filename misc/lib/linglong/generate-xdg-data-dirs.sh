# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# NOTE:
# Software installed by linglong (or linypas) are generally GUI applications,
# which should not override any existing files in origin XDG_DATA_DIRS,
# which are generally managed by system package manager like dpkg,
# to distribute system-wide service, desktop environment and applications.
# So we append the path to XDG_DATA_DIRS here like,
# instead of prepending it like flatpak.

# --- 变量初始化和规范化 ---
LINGLONG_ROOT="@LINGLONG_ROOT@"
LINGLONG_EXPORT_PATH="@LINGLONG_EXPORT_PATH@"

LINGLONG_DATA_DIR="${LINGLONG_ROOT}/entries/share"

# 如果 XDG_DATA_DIRS 为空，则先设置为 /usr/local/share:/usr/share
# 参考: https://specifications.freedesktop.org/basedir-spec/latest
if [ -z "$XDG_DATA_DIRS" ]; then
    XDG_DATA_DIRS="/usr/local/share:/usr/share"
fi

# --- 辅助函数：安全地添加路径到 XDG_DATA_DIRS ---
# 这个函数会检查路径是否已存在，如果不存在则添加。
# 参数1: 要添加的路径 (path_to_add)
# 参数2: 添加位置 ('start' 或 'end'，默认为 'start')
_add_path_to_xdg_data_dirs() {
    local path_to_add="${1%/}"   # 确保要添加的路径没有末尾斜杠
    local position="${2:-start}" # 默认添加到开头

    # 检查路径是否已存在于 XDG_DATA_DIRS 中
    # 在 XDG_DATA_DIRS 两端加上冒号，以确保精确匹配（无论路径在开头、中间还是结尾）
    if [[ ":${XDG_DATA_DIRS}:" == *":${path_to_add}:"* ]]; then
        # 路径已存在，不执行任何操作
        return 0
    fi

    if [[ "$position" == "start" ]]; then
        # 添加到开头
        XDG_DATA_DIRS="$path_to_add:$XDG_DATA_DIRS"
    elif [[ "$position" == "end" ]]; then
        # 添加到末尾
        XDG_DATA_DIRS="$XDG_DATA_DIRS:$path_to_add"
    fi
}

# 将 LINGLONG_DATA_DIR 添加到 XDG_DATA_DIRS 的末尾（如果不存在）
_add_path_to_xdg_data_dirs "$LINGLONG_DATA_DIR" "end"

# 如果有自定义 LINGLONG_EXPORT_PATH(默认为 "share"),将自定义路径添加到 XDG_DATA_DIRS
if [ "$LINGLONG_EXPORT_PATH" != "share" ]; then
    CUSTOM_DATA_DIR="${LINGLONG_ROOT}/entries/${LINGLONG_EXPORT_PATH}"
    _add_path_to_xdg_data_dirs "$CUSTOM_DATA_DIR" "start"
fi
export XDG_DATA_DIRS
# --- 清理辅助函数 ---
unset -f _add_path_to_xdg_data_dirs
