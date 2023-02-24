#!/bin/bash
#set -x

appId=$1
appEntriesPath=$2

desktopPath="${appEntriesPath}/share/applications"
systemdServicePath="${appEntriesPath}/share/systemd/user"
dbusServicePath="${appEntriesPath}/share/dbus-1"
contextMenuPath="${appEntriesPath}/share/applications/context-menus"

# replace 'Exec=XX' to 'Exec=ll-cli run ${appId} --exec "XX"'
# warning: the XX will include a prefix such as "/usr/bin".
ExecReplace()
{
    filePath=$1
    fileType=$2
    for path in `ls ${filePath}/${fileType}`
    do
        execContent=(`grep "^Exec=" ${path}`)
        IFS=$'\n'
        for content in ${execContent[@]}
        do
            if [[ "${content}" =~ "ll-cli run" ]];then
                continue
            fi
            startByLinglong="Exec=ll-cli run ${appId} --exec ${content/Exec=}"
            sed -i "s|${content}$|${startByLinglong}|g" ${path}
        done
    done
}

IconsReplace()
{
    echo "replace Icons"
}

DesktopGenerator()
{   
    ExecReplace ${desktopPath} "*.desktop"
    #IconsReplace ${desktopPath} "*.desktop"
}

DBusServiceGenerator()
{
    ExecReplace ${dbusServicePath} "*.service"
}

SystemdServiceGenerator()
{
    ExecReplace ${systemdServicePath} "*.service"
}

ContextMenuGenerator()
{
    ExecReplace ${contextMenuPath} "*.conf"
}

main () 
{
    if [ -d "${desktopPath}" ]; then
        DesktopGenerator
    fi
    if [ -d "${dbusServicePath}" ]; then
        DBusServiceGenerator
    fi
    if [ -d "${systemdServicePath}" ]; then
        SystemdServiceGenerator
    fi
    if [ -d "${contextMenuPath}" ]; then
        ContextMenuGenerator
    fi
}

main