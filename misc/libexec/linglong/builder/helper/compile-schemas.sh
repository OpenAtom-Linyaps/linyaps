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

# *.gschema.xml is the only input of glib-compile-schemas. When the directory
# holds no source file there is nothing to compile, and glib-compile-schemas
# would delete an existing gschemas.compiled instead of leaving it untouched,
# so bail out before calling it. The literal glob is kept when nothing matches.
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

# A failure here means the packaged schemas are broken, so it must fail the
# build instead of silently shipping an app whose gsettings keys are missing.
glib-compile-schemas "${schema_dir}"
