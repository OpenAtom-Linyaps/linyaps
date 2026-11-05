#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Automatically compile GSettings schemas during the build so that the
# resulting gschemas.compiled is shipped inside the package. Apps that
# depend on gsettings need this compiled binary to function at runtime
# without requiring glib-compile-schemas on the user's machine.
#
# Only the compiled gschemas.compiled is loaded by glib at runtime, so
# shipping the *.gschema.xml sources alone is not enough.

schema_dir="${PREFIX:-/usr}/share/glib-2.0/schemas"

if [ ! -d "${schema_dir}" ]; then
    exit 0
fi

# An unreadable directory cannot be inspected, and silently leaving the
# packaged schemas uncompiled would produce an app without its gsettings
# keys, so this has to fail the build.
if [ ! -r "${schema_dir}" ] || [ ! -x "${schema_dir}" ]; then
    echo "Error: cannot read ${schema_dir}, unable to compile schemas" >&2
    exit 1
fi

# Keep a precompiled schema binary shipped by the package as is.
if [ -e "${schema_dir}/gschemas.compiled" ]; then
    echo "${schema_dir}/gschemas.compiled already exists, skip compiling schemas"
    exit 0
fi

# Nothing to compile when the directory holds no schema source. Skipping also
# avoids invoking glib-compile-schemas on an empty directory and emitting a
# misleading missing-compiler warning. The literal glob is kept when nothing
# matches.
has_schema_source=false
for schema in "${schema_dir}"/*.gschema.xml; do
    if [ -e "${schema}" ]; then
        has_schema_source=true
        break
    fi
done

if [ "${has_schema_source}" = false ]; then
    echo "No *.gschema.xml in ${schema_dir}, skip compiling schemas"
    exit 0
fi

if ! command -v glib-compile-schemas >/dev/null 2>&1; then
    echo "Warning: glib-compile-schemas not found, ${schema_dir} is left uncompiled" >&2
    exit 0
fi

# Fatal errors make glib-compile-schemas exit non-zero, which fails the build
# through "set -e". Per-file schema problems are only warnings here.
glib-compile-schemas "${schema_dir}"
