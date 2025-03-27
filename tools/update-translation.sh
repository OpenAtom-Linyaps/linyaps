#!/bin/env bash

# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_UAB=ON -DSTATIC_BOX=ON -B build
cmake --build build --target pot
cmake --build build --target po
