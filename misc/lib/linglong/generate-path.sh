#!/usr/bin/env sh

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck shell=sh

# Append wrappers without shadowing system commands. Environment generators run
# before mounts are ready, so do not require entries/bin to exist yet.
_linglong_bin_dir="@LINGLONG_ROOT@/entries/bin"
case ":${PATH}:" in
    *":${_linglong_bin_dir}:"*) ;; # Already in PATH
    *) PATH="${PATH:+${PATH}:}${_linglong_bin_dir}" ;;
esac
unset _linglong_bin_dir
