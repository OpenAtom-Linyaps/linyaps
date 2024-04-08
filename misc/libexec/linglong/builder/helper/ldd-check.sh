#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

#ldd check binary script

declare failedList
declare -i total=0 pass=0 fail=0

if [ "$1" == "" ]; then
        echo "usage: lddscan.sh path or lddscan.sh [path1:path2:pathn]"
        exit -1
fi

#support multiple path, split by ':'
paths=$(echo $1 | tr ':' ' ')

filePaths=$(find $paths -type f)
IFS=$'\n'
for filePath in $filePaths; do
        mimeType=$(file $filePath --mime-type)
        isPieExe=$(echo $mimeType | grep "application/x-pie-executable")
        isSharedLib=$(echo $mimeType | grep "application/x-sharedlib")
        if [[ "$isPieExe" != "" || "$isSharedLib" != "" ]]; then
                echo "$filePath is ELF, checking ..."
                total+=1
                if [ "$(ldd $filePath | grep "not found")" != "" ]; then
                        echo "Warning! The ldd check is failed: $filePath"
                        fail+=1
                        failedList+="$filePath\n"
                        continue
                fi
                pass=$pass+1
        fi
done

echo "---------------------------"
echo "TOTAL: $total, PASS: $pass, FAIL: $fail"
echo "---------------------------"
if [ "$failedList" != "" ]; then
        echo -e "FAIL LIST:\n"
        echo -e "$failedList"
        echo "---------------------------"
        exit 1
fi
exit 0
