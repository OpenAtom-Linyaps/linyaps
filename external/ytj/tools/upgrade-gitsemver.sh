#!/bin/bash

set -e
set -o pipefail

GIT=${GIT:="git"}

repoRoot="$("$GIT" rev-parse --show-toplevel)"
cd "$repoRoot"

wget -O cmake/GitSemver.cmake https://github.com/black-desk/GitSemver.cmake/releases/latest/download/GitSemver.cmake
