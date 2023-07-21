#!/bin/env bash

GIT=${GIT:="git"}

cd "$("$GIT" rev-parse --show-toplevel)" || exit 255

IWYU_MAPGEN_QT=${IWYU_MAPGEN_QT:="iwyu-mapgen-qt"}

QT6_ROOT=${QT6_ROOT:="/usr/include/qt6"}

"$IWYU_MAPGEN_QT" "$QT6_ROOT" > tools/iwyu/iwyu-qt6_5.imp
