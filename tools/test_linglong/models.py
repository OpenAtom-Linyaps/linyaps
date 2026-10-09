#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
玲珑冒烟测试 — 数据类定义
"""

from dataclasses import dataclass, field


class StepSkipped(Exception):
    """用例的前置条件在当前环境里不满足 —— 不是被测代码的问题。

    典型场景：仓库/镜像里暂时查不到用例依赖的测试包（实测
    org.deepin.semver.demo 会随镜像同步时有时无），或者查远端仓库
    直接报错。这种情况必须【明确跳过并说明原因】，而不是让整轮
    fail-fast —— 否则后面几十个用例会全变成 SKIPPED，真正的问题
    反而被淹没。

    ⚠️ 只在"环境缺东西"时用；断言被测行为不对时一律用 AssertionError。
    """


@dataclass
class StepResult:
    index: int
    title: str
    status: str  # PASS / FAIL / SKIPPED
    duration_ms: int
    error_message: str = ""
    category: str = ""


@dataclass
class RepoState:
    default_repo: str = ""
    highest_priority_repo: str = ""
    highest_priority: int = 0
