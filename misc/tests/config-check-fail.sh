#!/bin/sh

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Fixture for main_check_test.cmake: a config check that always fails, so the
# test only exercises how main-check.sh propagates the failure at each level.
echo "stub: invalid application config"
exit 1
