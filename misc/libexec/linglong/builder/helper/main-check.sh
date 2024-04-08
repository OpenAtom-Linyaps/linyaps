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
ldd-check.sh "/usr/bin:/usr/lib:/opt/apps/:/runtime"
if [ ! $? -eq 0 ]; then
        echo "Error: ldd check failed."
        if [ $check_level -eq 1 ]; then
                exit 1
        fi
fi

echo "start application configure check"
config-check.sh

if [ ! $? -eq 0 ]; then
        echo "Warnning: configue check failed."
fi
