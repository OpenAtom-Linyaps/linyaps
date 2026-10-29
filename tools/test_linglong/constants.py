#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 模块级常量。"""

import os
from pathlib import Path

# ── repository ──
SMOKE_REPO_NAME = "smoketesting"
SMOKE_REPO_URL = "https://repo-dev.cicd.getdeepin.org"

# ── demo app ──
DEMO_APP_ID = "org.deepin.demo"
DEMO_VERSION = "0.0.0.1"
DEMO_ARCH = os.uname().machine
DEMO_CHANNEL = "main"
DEMO_PROJECT_DIR = DEMO_APP_ID
# 用于验证 `ll-builder build -f <file>`：把 linglong.yaml 复制一份、只改包名，
# 这样"到底读了哪个项目文件"在构建输出里就可辨认。
DEMO_CUSTOM_YAML_APP_ID = "org.deepin.customsmokefile"
# 用于验证 getProjectYAMLPath 的架构专属分支（linglong.<arch>.yaml 优先）。
DEMO_ARCH_YAML_APP_ID = "org.deepin.archsmokefile"

# ── calendar app ──
CALENDAR_APP_ID = "org.dde.calendar"
CALENDAR_VERSION = "5.13.1.1"
CALENDAR_MODULE_BASE_VERSION = "5.14.4.102"
CALENDAR_MODULE_DOWNGRADE_VERSION = "5.14.4.101"

# ── semver ──
SEMVER_APP_ID = "org.deepin.semver.demo"
SEMVER_OLD_VERSION = "1.0.0.0"

# ── baseline testsuite ──
TESTSUITE_BASELINE_APP_ID = "cn.org.linyaps.testsuite.baseline"
TESTSUITE_BASELINE_TIMEOUT = 300

# ── 非 TTY 的交互确认通知 ──
#
# ll-cli 需要用户确认时走 InteractiveNotifier：TTY 下直接问，非 TTY 下用
# cli/dbus_notifier.cpp 发桌面通知、等用户点按钮。而 sudo 会剥掉
# DBUS_SESSION_BUS_ADDRESS（本机没有 sudoers 的 env_keep），ll-cli 于是
# 退回 DummyNotifier —— 动作 "dummy" 被 Cli::interaction 当成 yes，
# dbus_notifier.cpp 一行都跑不到。
#
# 这里自己起一条 root 私有会话总线 + 桩通知服务（notif_stub.py），再用
# `sudo -A env DBUS_SESSION_BUS_ADDRESS=... ll-cli` 把地址显式传进去。
NOTIF_APP_ID = CALENDAR_APP_ID
NOTIF_BUS_DIR = "/tmp/linglong-smoke-notif-bus"
NOTIF_BUS_SOCKET = NOTIF_BUS_DIR + "/bus"
NOTIF_BUS_PID = NOTIF_BUS_DIR + "/daemon.pid"
NOTIF_STUB_LOG = NOTIF_BUS_DIR + "/notifications.jsonl"
NOTIF_STUB_OUT = NOTIF_BUS_DIR + "/stub.out"
NOTIF_STUB_PID = NOTIF_BUS_DIR + "/stub.pid"
NOTIF_INSTALL_TIMEOUT = 900

# ── 图形驱动检测 ──
LIBEXEC_DIR = "/usr/libexec/linglong"
DRIVER_DETECT_BINARY = "/usr/libexec/linglong/ll-driver-detect"

# ── tools ──
LL_CLI = "ll-cli"
LL_BUILDER = "ll-builder"

# ── repo 配置（机器状态）──
# 仓库增删、优先级、镜像开关、默认仓库都写在这个文件里。它是【机器状态】
# 而不是用例的私有数据，用例改完必须还原（见 SmokeTest.REPO_CONFIG_STEPS）。
REPO_CONFIG_PATH = Path("/var/lib/linglong/config.yaml")

# ── report ──
RESULTS_FILE = os.path.join(os.getcwd(), "test-results.json")
# Jenkins 的 junit 发布器直接吃这个文件（用例级结果/趋势），
# 和 xlsx / json 一样留在工作目录里。
JUNIT_FILE = os.path.join(os.getcwd(), "test-results-junit.xml")
