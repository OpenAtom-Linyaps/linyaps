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

LINGLONG_ROOT="@LINGLONG_ROOT@"

LINGLONG_DATA_DIR=${LINGLONG_ROOT}/entries/share
LINGLONG_DATA_DIR_DESKTOP=${LINGLONG_ROOT}/entries/linglong/share

# 初始化默认值（不覆盖已有值）
: "${XDG_DATA_DIRS:=/usr/local/share:/usr/share}"

# 检查是否需要添加 LINGLONG_DATA_DIR_DESKTOP
case ":${XDG_DATA_DIRS}:" in
    *":${LINGLONG_DATA_DIR_DESKTOP}:"* | *":${LINGLONG_DATA_DIR_DESKTOP}/:"*) ;;
    *)
        # 在 /usr/local/share 后插入（兼容末尾斜杠）
        XDG_DATA_DIRS=$(echo "$XDG_DATA_DIRS" | sed -E "s@(/usr/local/share/?)(:|$)@\1:$LINGLONG_DATA_DIR_DESKTOP\2@g")
        ;;
esac

# 检查是否需要添加 LINGLONG_DATA_DIR
case ":${XDG_DATA_DIRS}:" in
    *":${LINGLONG_DATA_DIR}:"* | *":${LINGLONG_DATA_DIR}/:"*) ;;
    *)
        XDG_DATA_DIRS="${XDG_DATA_DIRS}:${LINGLONG_DATA_DIR}"
        ;;
esac
