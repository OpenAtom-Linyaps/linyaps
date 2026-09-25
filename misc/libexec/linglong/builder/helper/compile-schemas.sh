#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Automatically compile GSettings schemas during the build so that the
# resulting gschemas.compiled is shipped inside the package. Apps that
# depend on gsettings need this compiled binary to function at runtime
# without requiring glib-compile-schemas on the user's machine.

schema_dir="$PREFIX/share/glib-2.0/schemas"

if [ ! -d "$schema_dir" ]; then
    exit 0
fi

if ! command -v glib-compile-schemas >/dev/null 2>&1; then
    echo "Warning: glib-compile-schemas not found, skip compiling schemas" >&2
    exit 0
fi

glib-compile-schemas "$schema_dir"
