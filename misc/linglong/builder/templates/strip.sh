#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

#binary strip script

if [ ! -f "$(command -v eu-strip)" ]; then
  echo "can not found  eu-strip"
  exit 1;
fi

#find binaries
find "${PREFIX}" -type f '(' -perm -111 -o -name '*.so*' -o -name '*.cmxs' \
                         -o -name '*.node' ')' \
                         -not -path "${PREFIX}/debug/*" \
                         > binaries

while IFS= read -r binary; do
#skip stripped binary
if [ "$(file "$binary"|grep ELF|grep 'not stripped')" == "" ]; then
  continue
fi

binaryName=${binary##*/}
binaryPath=${binary%/*}
#remove $PREFIX from binaryPath
subPath=${binaryPath/${PREFIX}}
destPath=${PREFIX}/debug/$subPath
extName=".debug"
debugFileName=$binaryName${extName}

mkdir -p "$destPath"
echo "strip $binary to $destPath/$debugFileName"
eu-strip "$binary" -f "$destPath/$debugFileName"
done < binaries
rm binaries
