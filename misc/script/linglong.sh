#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023-2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# Profile.d script for linglong/linyaps
# This script sources the host environment generator and exports the
# modified XDG_DATA_DIRS and PATH variables for user sessions.

source_script="@CMAKE_INSTALL_PREFIX@/lib/linglong/generate-xdg-data-dirs.sh"

# Source the script and export environment variables if successful
if [ -r "${source_script}" ]; then
    . "${source_script}"
    [ -n "${XDG_DATA_DIRS}" ] && export XDG_DATA_DIRS
    [ -n "${PATH}" ] && export PATH
fi
