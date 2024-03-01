#!/bin/bash

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"

wget -O cmake/GitSemver.cmake https://github.com/black-desk/GitSemver.cmake/releases/latest/download/GitSemver.cmake
wget -O cmake/PFL.cmake https://github.com/black-desk/PFL.cmake/releases/latest/download/PFL.cmake
wget -O cmake/CPM.cmake https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
