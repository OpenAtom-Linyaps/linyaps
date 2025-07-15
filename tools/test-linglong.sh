#!/bin/env bash

# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -xe

# 清理环境
rm -rf org.deepin.demo || true
ll-cli repo set-default stable
ll-cli repo remove smoketesting || true
ll-builder repo set-default stable
ll-builder repo remove smoketesting || true

echo "开始玲珑冒烟测试"

# 修改仓库为冒烟测试仓库
ll-cli repo add smoketesting https://repo-dev.cicd.getdeepin.org
ll-cli repo set-default smoketesting

ll-builder repo add smoketesting https://repo-dev.cicd.getdeepin.org
ll-builder repo set-default smoketesting

#创建玲珑项目
ll-builder create org.deepin.demo

#构建玲珑应用
pushd org.deepin.demo
ll-builder build

#导出layer文件
ll-builder export --layer

#导出uab文件
ll-builder export

#运行编译后的可执行程序
ll-builder run

# 测试dbus环境变量没有问题
ll-builder run -- bash -c "export" | grep DBUS_SESSION_BUS_ADDRESS
ll-builder run -- bash -c "export" | grep DBUS_SYSTEM_BUS_ADDRESS
# 测试session dbus环境变量包含参数时能正常处理
export DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS,test=1
ll-builder run -- bash -c "export" | grep DBUS_SESSION_BUS_ADDRESS | grep test=1
# 测试system dbus环境变量包含参数时能正常处理
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=/var/run/dbus/system_bus_socket,test=2"
ll-builder run -- bash -c "export" | grep DBUS_SYSTEM_BUS_ADDRESS | grep test=2

#运行并安装uab
ll-cli uninstall org.deepin.demo || true
./org.deepin.demo_x86_64_0.0.0.1_main.uab

#安装构建的应用
ll-cli install org.deepin.demo_x86_64_0.0.0.1_main.uab
ll-cli uninstall org.deepin.demo || true
ll-cli install org.deepin.demo_0.0.0.1_x86_64_binary.layer

#运行安装好的demo
timeout 10 ll-cli run org.deepin.demo
popd
rm -rf org.deepin.demo
ll-cli uninstall org.deepin.demo

#列出已经安装的应用
ll-cli list

#从远程仓库查询应用
ll-cli search .

#通过 ll-cli search命令可以从远程 repo 中查找应用程序信息
ll-cli search calendar

#查找 Base 和 Runtime
ll-cli search . --type=runtime

appid=org.dde.calendar
test_version=5.13.1.1
#先卸载再安装
ll-cli uninstall "$appid" || true

#ll-cli install命令用来安装玲珑应用
ll-cli install "$appid"

#先卸载再安装
ll-cli uninstall "$appid"

#安装指定版本
ll-cli install "$appid/$test_version"

#更新新版本
ll-cli upgrade "$appid"

#运行玲珑应用
ll-cli run "$appid" &
sleep 5

ll-cli kill -9 "$appid"
sleep 3
ll-cli uninstall org.dde.calendar

# 测试module安装
ll-cli install org.dde.calendar/5.14.4.102
ll-cli install --module develop org.dde.calendar
ll-cli install --module unuse org.dde.calendar
ll-cli install --module lang-ja org.dde.calendar
# 101版本没有unuse模块，降级后删除unuse模块，保留其他模块
ll-cli install --force org.dde.calendar/5.14.4.101
ll-cli list | grep org.dde.calendar | grep -q binary
ll-cli list | grep org.dde.calendar | grep -q develop
ll-cli list | grep org.dde.calendar | grep -q lang-ja
ll-cli list | grep org.dde.calendar | grep -vq unuse
# 最新版本没有lang-ja模块，升级后删除lang-ja模块，保留其他模块
ll-cli upgrade org.dde.calendar
ll-cli list | grep org.dde.calendar | grep -q binary
ll-cli list | grep org.dde.calendar | grep -q develop
ll-cli list | grep org.dde.calendar | grep -vq lang-ja

#测试versionV1 升级到 versionV2
echo "测试versionV1 升级到 versionV2"
ll-cli search org.deepin.semver.demo

# 安装
echo "安装"
ll-cli install org.deepin.semver.demo
ll-cli list | grep org.deepin.semver.demo

# 升级
echo "升级"
ll-cli uninstall org.deepin.semver.demo
ll-cli install org.deepin.semver.demo/1.0.0.0
ll-cli upgrade org.deepin.semver.demo
ll-cli list | grep org.deepin.semver.demo

# 降级
echo "降级"
ll-cli uninstall org.deepin.semver.demo
ll-cli install org.deepin.semver.demo/1.0.0.0 --force
ll-cli list | grep org.deepin.semver.demo

#重置默认仓库
ll-cli repo set-default stable
ll-cli uninstall org.deepin.semver.demo

echo "成功执行玲珑冒烟测试"
