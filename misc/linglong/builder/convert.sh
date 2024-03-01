#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

LINGLONG_BUILDER="/usr/bin/ll-builder"

if [ -f "$LINGLONG_BUILDER" ]; then
	if ! $LINGLONG_BUILDER build; then
		echo "failed to build linglong pkg."
	fi

	if $LINGLONG_BUILDER export; then
		echo "success to convert linglong pkg success!"
	fi

	find . -maxdepth 1 -not -regex '.*\.\|.*\.layer\|.*\.uab' -exec basename {} -print0 \;  | xargs rm -r
fi
