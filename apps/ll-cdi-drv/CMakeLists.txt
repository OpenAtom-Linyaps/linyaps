# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  OUTPUT_NAME
  ll-cdi-drv
  LIBEXEC
  linglong
  SOURCES
  ./src/main.cpp
  LINK_LIBRARIES
  PRIVATE
  CLI11::CLI11
  linglong::cdi_drv
  linglong::utils)
