# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# TODO: if erofsfuse and libdeflate provides .pc file or config.cmake, change it
# to pkg_search_modules/find_package

find_library(FOUND_erofsfuse_STATIC "erofsfuse")
if("${FOUND_erofsfuse_STATIC}" STREQUAL "")
  message(FATAL_ERROR "liberofsfuse.a not found")
endif()

get_filename_component(EROFSFUSE_ABS_FILE ${FOUND_erofsfuse_STATIC} ABSOLUTE)
message(STATUS "found liberofsfuse.a: ${ABS_FILE}")

find_library(FOUND_libdeflate_STATIC "deflate")
if("${FOUND_libdeflate_STATIC}" STREQUAL "")
  message(FATAL_ERROR "libdeflate.a not found")
endif()

get_filename_component(LIBDEFLATE_ABS_FILE ${FOUND_libdeflate_STATIC} ABSOLUTE)
message(STATUS "found libdeflate.a: ${LIBDEFLATE_ABS_FILE}")

pkg_search_module(elf_static REQUIRED IMPORTED_TARGET libelf)
pkg_search_module(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_search_module(lzma REQUIRED IMPORTED_TARGET liblzma)
pkg_search_module(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_search_module(ZSTD REQUIRED IMPORTED_TARGET libzstd)
pkg_search_module(FUSE REQUIRED IMPORTED_TARGET fuse)
pkg_search_module(SELINUX REQUIRED IMPORTED_TARGET libselinux)

target_link_libraries(PkgConfig::SELINUX INTERFACE ${SELINUX_STATIC_LIBRARIES})
target_link_libraries(PkgConfig::FUSE INTERFACE ${FUSE_STATIC_LIBRARIES})

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  ./src/main.cpp
  ./src/sha256.h
  OUTPUT_NAME
  uab-header
  LINK_LIBRARIES
  PRIVATE
  linglong::api
  nlohmann_json::nlohmann_json
  PkgConfig::SELINUX
  PkgConfig::elf_static
  PkgConfig::lz4
  PkgConfig::lzma
  PkgConfig::ZLIB
  PkgConfig::ZSTD
  PkgConfig::FUSE
  ${EROFSFUSE_ABS_FILE}
  ${LIBDEFLATE_ABS_FILE}
  stdc++fs)

set(UAB_HEADER_TARGET)
get_real_target_name(UAB_HEADER_TARGET linglong::header)

target_link_options(${UAB_HEADER_TARGET} PRIVATE -static -static-libgcc
                    -static-libstdc++)

if(${AGGRESSIVE_UAB_SIZE})
  target_compile_options(
    ${UAB_HEADER_TARGET} PRIVATE -Os -fno-asynchronous-unwind-tables -fno-rtti
                                 -fdata-sections -ffunction-sections -flto=auto)

  target_link_options(
    ${UAB_HEADER_TARGET} PRIVATE
    -Wl,--gc-sections,--strip-all,--exclude-libs,ALL -flto=auto)
endif()

include(GNUInstallDirs)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uab-header
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/linglong/builder/uab)
