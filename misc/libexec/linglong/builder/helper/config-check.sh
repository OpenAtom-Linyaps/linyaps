#!/bin/bash

# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

#check app config file script
#config file type: desktop|context-menu|dbus service| systemd user service

if [ ! -d "/opt/apps/${LINGLONG_APPID}" ]; then
        echo "/opt/apps/$APPID is not exist."
        exit 255
fi

PREFIX="/opt/apps/${LINGLONG_APPID}/files"

declare results invalidList

desktop=$PREFIX/share/applications/
contextMenu=$PREFIX/share/applications/context-menus
dbusService=$PREFIX/share/dbus-1/services
systemdUserService=$PREFIX/lib/systemd/user

if [ -d "$desktop" ]; then
        results+=$(find "$desktop" -name "*.desktop")
        results+="\n"
fi
if [ -d "$contextMenu" ]; then
        results+=$(find "$contextMenu" -name "*.conf")
        results+="\n"
fi
if [ -d "$dbusService" ]; then
        results+=$(find "$dbusService" -name "*.service")
        results+="\n"
fi
if [ -d "$systemdUserService" ]; then
        results+=$(
                find "$systemdUserService" -name "*.service" \
                        -o -name "*.socket" \
                        -o -name "*.device" \
                        -o -name "*.mount" \
                        -o -name "*.automount" \
                        -o -name "*.swap" \
                        -o -name "*.target" \
                        -o -name "*.path" \
                        -o -name "*.timer" \
                        -o -name "*.slice" \
                        -o -name "*.scope"
        )
fi

for filePath in $(echo -e "$results"); do
        fileName=${filePath##*/}
        ret=$(echo "$fileName" | grep "^${LINGLONG_APPID}")
        if [ "$ret" == "" ]; then
                invalidList+="$filePath\n"
        fi
done

if [ "$invalidList" != "" ]; then
        echo -e "These files have invalid file names:\n"
        echo -e "$invalidList"
        echo -e "We prefer to use \$LINGLONG_APPID as the prefix. Such as ${LINGLONG_APPID}.xxx."
        exit 1
fi

exit 0
