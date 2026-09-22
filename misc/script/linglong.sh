#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# Profile.d script for linglong/linyaps (session-leg PATH/XDG injection)
# This script is the "session leg" of a dual-leg design:
#   - manager leg: systemd user environment generator (61-linglong) runs at
#     session start via systemd, covering greeter-first topologies.
#   - session leg: this profile.d script runs when a shell or X session
#     starts, covering topologies where the session leg overrides the
#     manager leg (e.g. Plasma/lightdm+SDDM).
# Both legs inject entries/bin into PATH and source the XDG_DATA_DIRS
# generation script. Having both ensures coverage across all session
# topologies.

# Inject entries/bin into PATH (append, not prepend, to avoid shadowing
# system commands like git/python with wrapper scripts).
_linglong_bin_dir="@LINGLONG_ROOT@/entries/bin"
if [ -n "${_linglong_bin_dir}" ] && [ -d "${_linglong_bin_dir}" ]; then
    case ":${PATH}:" in
        *":${_linglong_bin_dir}:"*) ;; # Already in PATH
        *) PATH="${PATH:+${PATH}:}${_linglong_bin_dir}"; export PATH ;;
    esac
fi
unset _linglong_bin_dir

source_script="@CMAKE_INSTALL_PREFIX@/lib/linglong/generate-xdg-data-dirs.sh"

# Source the script and export XDG_DATA_DIRS if successful
[ -r "${source_script}" ] && . "${source_script}" && [ -n "${XDG_DATA_DIRS}" ] && export XDG_DATA_DIRS
