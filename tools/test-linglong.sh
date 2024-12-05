#!/bin/env bash

# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -xe

#创建玲珑项目
ll-builder create org.dde.demo

#构建玲珑应用
pushd org.dde.demo
ll-builder build

#导出layer文件
ll-builder export --layer

#导出uab文件
ll-builder export

#运行编译后的可执行程序
ll-builder run

#运行并安装uab
sudo ll-cli uninstall org.dde.demo || true
./org.dde.demo_x86_64_0.0.0.1_main.uab

#安装构建的应用
sudo ll-cli uninstall org.dde.demo || true
ll-cli install org.dde.demo_0.0.0.1_x86_64_binary.layer

#运行安装好的demo
timeout 10 ll-cli run org.dde.demo
popd
rm -rf org.dde.demo
ll-cli uninstall org.dde.demo

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
sudo ll-cli uninstall "$appid" || true

#ll-cli install命令用来安装玲珑应用
sudo ll-cli install "$appid"

#先卸载再安装
sudo ll-cli uninstall "$appid"

#安装指定版本
sudo ll-cli install "$appid/$test_version"

#更新新版本
sudo ll-cli upgrade "$appid"

#运行玲珑应用
ll-cli run "$appid" &
sleep 20

cur_version=$(ll-cli info "$appid" | jq '.version' | xargs)
arch=$(uname -m)
ll-cli kill "main:$appid/$cur_version/$arch"
