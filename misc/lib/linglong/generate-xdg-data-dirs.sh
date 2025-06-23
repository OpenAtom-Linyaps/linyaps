#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023-2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# NOTE:
# Software installed by linglong (or linypas) are generally GUI applications,
# which should not override any existing files in origin XDG_DATA_DIRS,
# which are generally managed by system package manager like dpkg,
# to distribute system-wide service, desktop environment and applications.
# So we append the path to XDG_DATA_DIRS here like,
# instead of prepending it like flatpak.

# --- Variable initialization ---
LINGLONG_ROOT="@LINGLONG_ROOT@"
LINGLONG_EXPORT_PATH="@LINGLONG_EXPORT_PATH@"
LINGLONG_DATA_DIR="${LINGLONG_ROOT}/entries/share"

# --- Helper function: safely add path to XDG_DATA_DIRS ---
# Parameters: path_to_add [position]
# position: "begin" to add at the beginning, anything else (or omitted) to add at the end
_append_path_to_xdg_data_dirs() {
    path_to_add="$1"
    position="${2:-end}"

    # Remove trailing slash if present
    case "${path_to_add}" in
    */) path_to_add="${path_to_add%/}" ;;
    esac

    # Check if path is empty
    [ -z "${path_to_add}" ] && return 0

    # Check if path already exists in XDG_DATA_DIRS
    [ -n "${XDG_DATA_DIRS}" ] && case ":${XDG_DATA_DIRS}:" in
    *":${path_to_add}:"*) return 0 ;; # Path already exists
    esac

    # Add to the beginning or end based on position parameter
    case "${position}" in
    "begin")
        XDG_DATA_DIRS="${path_to_add}${XDG_DATA_DIRS:+:${XDG_DATA_DIRS}}"
        ;;
    *)
        XDG_DATA_DIRS="${XDG_DATA_DIRS:+${XDG_DATA_DIRS}:}${path_to_add}"
        ;;
    esac
}

# --- Helper function: initialize XDG_DATA_DIRS if empty ---
_init_xdg_data_dir() {
    # If XDG_DATA_DIRS is empty, set to system default paths
    [ -z "${XDG_DATA_DIRS}" ] && XDG_DATA_DIRS="/usr/local/share:/usr/share"
}

_init_xdg_data_dir

# Add LINGLONG_DATA_DIR to the end of XDG_DATA_DIRS (if it doesn't exist)
_append_path_to_xdg_data_dirs "${LINGLONG_DATA_DIR}"

# If there's a custom LINGLONG_EXPORT_PATH (default is "share"), add custom path to XDG_DATA_DIRS
[ "${LINGLONG_EXPORT_PATH}" != "share" ] && _append_path_to_xdg_data_dirs "${LINGLONG_ROOT}/entries/${LINGLONG_EXPORT_PATH}" "begin"

# --- Clean up helper functions ---
unset -f _append_path_to_xdg_data_dirs _init_xdg_data_dir
