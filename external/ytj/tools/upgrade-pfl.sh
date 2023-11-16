#!/bin/bash

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"

wget -O cmake/PFL.cmake https://github.com/black-desk/PFL.cmake/releases/latest/download/PFL.cmake
