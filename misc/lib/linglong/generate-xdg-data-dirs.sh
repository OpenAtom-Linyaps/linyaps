#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2023-2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# NOTE:
# Linglong exports desktop integration data and command wrappers from
# ${LINGLONG_ROOT}/entries. We append those paths so host-managed files keep
# higher priority while exported apps remain discoverable.

# --- Variable initialization ---
LINGLONG_ROOT="@LINGLONG_ROOT@"
LINGLONG_EXPORT_PATH="@LINGLONG_EXPORT_PATH@"
LINGLONG_DATA_DIR="${LINGLONG_ROOT}/entries/share"
LINGLONG_BIN_DIR="${LINGLONG_ROOT}/entries/bin"

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

# --- Helper function: safely add path to PATH ---
_append_path_to_path() {
    path_to_add="$1"

    case "${path_to_add}" in
    */) path_to_add="${path_to_add%/}" ;;
    esac

    [ -z "${path_to_add}" ] && return 0

    [ -n "${PATH}" ] && case ":${PATH}:" in
    *":${path_to_add}:"*) return 0 ;;
    esac

    PATH="${PATH:+${PATH}:}${path_to_add}"
}

# --- Helper function: initialize PATH if empty ---
_init_path() {
    [ -z "${PATH}" ] && PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
}

_init_xdg_data_dir
_init_path

# Add LINGLONG_DATA_DIR to the end of XDG_DATA_DIRS (if it doesn't exist)
_append_path_to_xdg_data_dirs "${LINGLONG_DATA_DIR}"

# If there's a custom LINGLONG_EXPORT_PATH (default is "share"), add custom path to XDG_DATA_DIRS
[ "${LINGLONG_EXPORT_PATH}" != "share" ] && _append_path_to_xdg_data_dirs "${LINGLONG_ROOT}/entries/${LINGLONG_EXPORT_PATH}" "begin"

# Export command wrappers from ${LINGLONG_ROOT}/entries/bin via PATH.
_append_path_to_path "${LINGLONG_BIN_DIR}"

# --- Clean up helper functions ---
unset -f _append_path_to_xdg_data_dirs _init_xdg_data_dir _append_path_to_path _init_path
