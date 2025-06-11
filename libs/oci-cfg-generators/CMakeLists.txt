# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  # find -regex '\.\/.+\.[ch]\(pp\)?' -type f -printf '%P\n'| sort
  src/linglong/oci-cfg-generators/container_cfg_builder.cpp
  src/linglong/oci-cfg-generators/container_cfg_builder.h
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  linglong::ocppi
  linglong::api
  )

get_real_target_name(GEN linglong::oci-cfg-generators)
target_include_directories(${GEN} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src/linglong/oci-cfg-generators)
