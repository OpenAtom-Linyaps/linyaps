#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# main check script

#default level is 1
declare -i check_level=1
declare -i skip_ldd=0
declare -i skip_config=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-ldd-check)
            skip_ldd=1
            shift
            ;;
        --skip-config-check)
            skip_config=1
            shift
            ;;
        *)
            if [[ -n "$1" ]]; then
                check_level=$1
            fi
            shift
            ;;
    esac
done

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

if [[ $skip_ldd -eq 0 ]]; then
    echo "start ldd check"
    if ! @CMAKE_INSTALL_FULL_LIBEXECDIR@/linglong/builder/helper/ldd-check.sh "/opt/apps/${LINGLONG_APPID}/files"; then
            echo "Error: ldd check failed."
            if [ $check_level -eq 1 ]; then
                    exit 1
            fi
    fi
else
    echo "Skipping ldd check."
fi

if [[ $skip_config -eq 0 ]]; then
    echo "start application configure check"
    if ! @CMAKE_INSTALL_FULL_LIBEXECDIR@/linglong/builder/helper/config-check.sh; then
            echo "Warning: application configure check failed."
    fi
else
    echo "Skipping application configure check."
fi
