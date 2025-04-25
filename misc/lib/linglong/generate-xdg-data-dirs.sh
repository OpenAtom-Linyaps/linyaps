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
LINGLONG_DESKTOP_EXPORT_PATH="@LINGLONG_DESKTOP_EXPORT_PATH@"

LINGLONG_DATA_DIR=${LINGLONG_ROOT}/entries/share
CUSTOM_DESKTOP_DATA_DIR=${LINGLONG_ROOT}/entries/${LINGLONG_DESKTOP_EXPORT_PATH}

if [ "$LINGLONG_DESKTOP_EXPORT_PATH" != "share/applications" ]; then
    # 检查XDG_DATA_DIRS是否包含CUSTOM_DESKTOP_DATA_DIR
    case ":$XDG_DATA_DIRS:" in
    *":${CUSTOM_DESKTOP_DATA_DIR}:"* | *":${CUSTOM_DESKTOP_DATA_DIR}/:"*) ;;
    *)
        XDG_DATA_DIRS=$(echo "$XDG_DATA_DIRS" | sed -E "s@(/usr/local/share/?)(:|$)@\1:$CUSTOM_DESKTOP_DATA_DIR\2@g")
        ;;
    esac
else
    case ":$XDG_DATA_DIRS:" in
    *":$LINGLONG_DATA_DIR:"* | *":$LINGLONG_DATA_DIR/:"*) : ;;
    *)
        XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:${LINGLONG_DATA_DIR}"
        ;;
    esac
fi
