# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  SOURCES
  ./src/main.cpp
  OUTPUT_NAME
  ll-cli
  LINK_LIBRARIES
  PUBLIC
  linglong
  docopt)
