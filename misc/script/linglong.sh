#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# Profile.d and Xsession.d script for linglong/linyaps.
# Restore PATH after login scripts reset it, and export both variables for
# desktop applications, shells, and the session's environment import.

source_script="@CMAKE_INSTALL_PREFIX@/lib/linglong/generate-path.sh"
[ -r "${source_script}" ] && . "${source_script}" && [ -n "${PATH}" ] && export PATH

source_script="@CMAKE_INSTALL_PREFIX@/lib/linglong/generate-xdg-data-dirs.sh"

# Source the script and export XDG_DATA_DIRS if successful
[ -r "${source_script}" ] && . "${source_script}" && [ -n "${XDG_DATA_DIRS}" ] && export XDG_DATA_DIRS
