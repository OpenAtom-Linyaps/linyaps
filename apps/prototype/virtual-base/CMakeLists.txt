# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_executable(ll-prototype-virtual-base)
aux_source_directory(. LL_PVB_SOURCES)
target_sources(ll-prototype-virtual-base PRIVATE ${LL_PVB_SOURCES})
target_link_libraries(
  ll-prototype-virtual-base
  Qt::Core
)
