# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

macro(configure_patch_variable source)
  get_filename_component(BASE_NAME ${source} NAME_WE)
  string(TOUPPER ${BASE_NAME} PATCH_VAR)
  string(REPLACE "-" "_" PATCH_VAR ${PATCH_VAR})
  string(PREPEND PATCH_VAR "LINGLONG_OCI_CFG_GEN_")
  file(READ ${source} ${PATCH_VAR})
endmacro()

configure_patch_variable(${PROJECT_SOURCE_DIR}/misc/lib/linglong/container/config.json)
configure_patch_variable(${PROJECT_SOURCE_DIR}/misc/lib/linglong/container/config.d/10-basics.json)
configure_patch_variable(${PROJECT_SOURCE_DIR}/misc/lib/linglong/container/config.d/25-host-rootfs.json)
configure_patch_variable(${PROJECT_SOURCE_DIR}/misc/lib/linglong/container/config.d/25-host-statics.json)

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  # find -regex '\.\/.+\.[ch]\(pp\)?' -type f -printf '%P\n'| sort
  src/linglong/oci-cfg-generators/00_id_mapping.cpp
  src/linglong/oci-cfg-generators/00_id_mapping.h
  src/linglong/oci-cfg-generators/05_initialize.cpp
  src/linglong/oci-cfg-generators/05_initialize.h
  src/linglong/oci-cfg-generators/10_basics.cpp
  src/linglong/oci-cfg-generators/10_basics.h.in
  src/linglong/oci-cfg-generators/20_devices.cpp
  src/linglong/oci-cfg-generators/20_devices.h
  src/linglong/oci-cfg-generators/25_host_env.cpp
  src/linglong/oci-cfg-generators/25_host_env.h
  src/linglong/oci-cfg-generators/25_host_rootfs.cpp
  src/linglong/oci-cfg-generators/25_host_rootfs.h.in
  src/linglong/oci-cfg-generators/25_host_statics.cpp
  src/linglong/oci-cfg-generators/25_host_statics.h.in
  src/linglong/oci-cfg-generators/30_user_home.cpp
  src/linglong/oci-cfg-generators/30_user_home.h
  src/linglong/oci-cfg-generators/40_host_ipc.cpp
  src/linglong/oci-cfg-generators/40_host_ipc.h
  src/linglong/oci-cfg-generators/90_legacy.cpp
  src/linglong/oci-cfg-generators/90_legacy.h
  src/linglong/oci-cfg-generators/builtins.h.in
  src/linglong/oci-cfg-generators/container_cfg_builder.cpp
  src/linglong/oci-cfg-generators/container_cfg_builder.h
  src/linglong/oci-cfg-generators/generator.h
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
