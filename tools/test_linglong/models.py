#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
玲珑冒烟测试 — 数据类定义
"""

from dataclasses import dataclass, field


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
