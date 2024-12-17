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
  src/linglong/oci-cfg-generators/00_id_mapping.cpp
  src/linglong/oci-cfg-generators/00_id_mapping.h
  src/linglong/oci-cfg-generators/05_initialize.cpp
  src/linglong/oci-cfg-generators/05_initialize.h
  src/linglong/oci-cfg-generators/10_basics.cpp
  src/linglong/oci-cfg-generators/10_basics.h
  src/linglong/oci-cfg-generators/20_devices.cpp
  src/linglong/oci-cfg-generators/20_devices.h
  src/linglong/oci-cfg-generators/25_host_env.cpp
  src/linglong/oci-cfg-generators/25_host_env.h
  src/linglong/oci-cfg-generators/25_host_rootfs.cpp
  src/linglong/oci-cfg-generators/25_host_rootfs.h
  src/linglong/oci-cfg-generators/25_host_statics.cpp
  src/linglong/oci-cfg-generators/25_host_statics.h
  src/linglong/oci-cfg-generators/30_user_home.cpp
  src/linglong/oci-cfg-generators/30_user_home.h
  src/linglong/oci-cfg-generators/40_host_ipc.cpp
  src/linglong/oci-cfg-generators/40_host_ipc.h
  src/linglong/oci-cfg-generators/90_legacy.cpp
  src/linglong/oci-cfg-generators/90_legacy.h
  src/linglong/oci-cfg-generators/builtins.h
  src/linglong/oci-cfg-generators/generator.h
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  linglong::ocppi
  linglong::api
  )
