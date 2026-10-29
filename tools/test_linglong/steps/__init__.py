#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""冒烟测试步骤分组。

每个模块只放一组步骤的方法；步骤表（含依赖声明）统一在 main.py 里，
因为步骤的执行顺序有语义（demo 项目生命周期、prune 等），不能按组重排。
"""
