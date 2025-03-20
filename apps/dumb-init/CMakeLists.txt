# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  SOURCES
  ./src/dumb-init.c
  OUTPUT_NAME
  dumb-init
  LIBEXEC
  linglong
)

set(DUMB_INIT_TARGET)

get_real_target_name(DUMB_INIT_TARGET linglong::dumb-init)

target_link_options(${DUMB_INIT_TARGET} PRIVATE -static)
