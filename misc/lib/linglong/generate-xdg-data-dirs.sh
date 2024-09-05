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
case ":$XDG_DATA_DIRS:" in
*":$LINGLONG_DATA_DIR:"*) : ;;
*":$LINGLONG_DATA_DIR/:"*) : ;;
*)
        XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:${LINGLONG_DATA_DIR}"
        ;;
esac
