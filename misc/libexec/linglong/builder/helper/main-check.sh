#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# main check script

#default level is 1
declare -i check_level=1

if [[ $1 != "" ]]; then
        check_level=$1
fi

case $check_level in
0)
        echo "The check level is 0, some checks failed will be ignored."
        ;;
1)
        echo "The check level is 1, some checks failed will be treated as error."
        ;;
*)
        echo "Invalid level."
        exit 1
        ;;
esac

echo "start ldd check"
if ! /usr/libexec/linglong/builder/helper/ldd-check.sh "/opt/apps/${LINGLONG_APPID}/files"; then
        echo "Error: ldd check failed."
        if [ $check_level -eq 1 ]; then
                exit 1
        fi
fi

echo "start application configure check"
if ! /usr/libexec/linglong/builder/helper/config-check.sh; then
        echo "Warnning: configue check failed."
fi
