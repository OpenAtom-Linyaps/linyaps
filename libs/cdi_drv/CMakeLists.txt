# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  src/linglong/cdi_drv/config.cpp
  src/linglong/cdi_drv/config.h
  src/linglong/cdi_drv/drm.cpp
  src/linglong/cdi_drv/drm.h
  src/linglong/cdi_drv/generate.cpp
  src/linglong/cdi_drv/generate.h
  src/linglong/cdi_drv/refresh.cpp
  src/linglong/cdi_drv/refresh.h
  src/linglong/cdi_drv/requests.h
  src/linglong/cdi_drv/watch.cpp
  src/linglong/cdi_drv/watch.h
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  linglong::utils
  PRIVATE
  linglong::cdi)

if(CMAKE_LIBRARY_ARCHITECTURE)
  set(_cdi_drv_default_lib_dir "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
elseif((NOT CMAKE_CROSSCOMPILING OR CMAKE_SYSROOT)
       AND IS_DIRECTORY "${CMAKE_SYSROOT}/usr/lib64")
  set(_cdi_drv_default_lib_dir "/usr/lib64")
else()
  set(_cdi_drv_default_lib_dir "/usr/lib")
endif()

set(LINGLONG_CDI_DRV_LIB_DIR "${_cdi_drv_default_lib_dir}" CACHE PATH
    "Target system library directory used by the CDI driver generator")

get_real_target_name(CDI_DRV_TARGET linglong::cdi_drv)
if(HAVE_SYSTEMD)
  target_link_libraries(${CDI_DRV_TARGET} PRIVATE PkgConfig::systemd)
endif()
target_compile_definitions(
  ${CDI_DRV_TARGET} PRIVATE
  LINGLONG_CDI_DRV_LIB_DIR="${LINGLONG_CDI_DRV_LIB_DIR}")
