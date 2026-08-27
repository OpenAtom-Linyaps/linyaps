#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023-2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# Profile.d script for linglong/linyaps
# This script injects entries/bin into PATH and sources the XDG_DATA_DIRS
# generation script, exporting the modified environment variables for user sessions.

# Inject entries/bin into PATH so exported binary wrapper scripts are accessible
# from the user's shell session.
_linglong_bin_dir="@LINGLONG_ROOT@/entries/bin"
if [ -n "${_linglong_bin_dir}" ] && [ -d "${_linglong_bin_dir}" ]; then
    case ":${PATH}:" in
    *":${_linglong_bin_dir}:"*) ;; # Already in PATH
    *) PATH="${_linglong_bin_dir}:${PATH}" && export PATH ;;
    esac
fi
unset _linglong_bin_dir

source_script="@CMAKE_INSTALL_PREFIX@/lib/linglong/generate-xdg-data-dirs.sh"

# Source the script and export XDG_DATA_DIRS if successful
[ -r "${source_script}" ] && . "${source_script}" && [ -n "${XDG_DATA_DIRS}" ] && export XDG_DATA_DIRS
