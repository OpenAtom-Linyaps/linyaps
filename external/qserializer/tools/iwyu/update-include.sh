#!/bin/env bash

GIT=${GIT:="git"}

cd "$("$GIT" rev-parse --show-toplevel)" || exit 255

IWYU_TOOL=${IWYU_TOOL:="iwyu-tool"}

"$IWYU_TOOL" -p build -- \
	-Xiwyu --mapping_file="$(pwd)"/tools/iwyu/iwyu-qt6_5.imp |
	tee build/iwyu.out

IWYU_FIX_INCLUDES=${IWYU_FIX_INCLUDES:="iwyu-fix-includes"}

"$IWYU_FIX_INCLUDES" --ignore_re 'build/*' \
	--update_comments \
	<build/iwyu.out

CLANG_FORMAT=${CLANG_FORMAT:="clang-format"}

find . -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' \
	-exec "$CLANG_FORMAT" -style=file -i {} \;
