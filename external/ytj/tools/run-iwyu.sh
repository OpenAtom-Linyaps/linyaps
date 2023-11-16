#!/bin/env bash

GIT=${GIT:="git"}

cd "$("$GIT" rev-parse --show-toplevel)" || exit 255

IWYU_TOOL=${IWYU_TOOL:=$({
	command -v iwyu_tool &>/dev/null &&
		echo "iwyu_tool" && exit
	command -v iwyu-tool &>/dev/null &&
		echo "iwyu-tool" && exit
	echo iwyu-tool
})}

cmake \
	-B build-iwyu \
	-DFETCHCONTENT_QUIET=OFF \
	-DOCPPI_INSTALL=NO \
	-DCPM_DOWNLOAD_ALL=YES \
	-DCMAKE_CXX_STANDARD=17 \
	-DCMAKE_CXX_STANDARD_REQUIRED=YES \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
	-DCMAKE_CXX_COMPILER=/usr/bin/clang++

# shellcheck disable=SC2046
"$IWYU_TOOL" -p build-iwyu \
	$(
		find . -path './libs*' \
			\( -name '*.c' -o -name '*.cpp' \) \
			-printf "%p "
	) |
	tee build/iwyu.out

IWYU_FIX_INCLUDES=${IWYU_FIX_INCLUDES:=$({
	command -v fix_include &>/dev/null &&
		echo "fix_include" && exit
	command -v iwyu-fix-includes &>/dev/null &&
		echo "iwyu-fix-includes" && exit
	echo iwyu-fix-includes
})}

"$IWYU_FIX_INCLUDES" \
	--ignore_re \
	'(\.cache|build)\/*' \
	--update_comments \
	--nosafe_headers \
	<build/iwyu.out

CLANG_FORMAT=${CLANG_FORMAT:=$({
	command -v clang-format-16 &>/dev/null &&
		echo "clang-format-16" && exit
	command -v clang-format &>/dev/null &&
		echo "clang-format" && exit
	echo "clang-format"
})}

find . -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)' \
	-exec "$CLANG_FORMAT" -style=file -i {} \;
