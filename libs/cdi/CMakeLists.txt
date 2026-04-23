# SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  # find -regex '\.\/.+\.[ch]\(pp\)?' -type f -printf '%P\n'| sort
  src/linglong/cdi/cdi.cpp
  src/linglong/cdi/cdi.h
  src/linglong/cdi/types/Cdi.hpp
  src/linglong/cdi/types/ContainerEdits.hpp
  src/linglong/cdi/types/Device.hpp
  src/linglong/cdi/types/DeviceNode.hpp
  src/linglong/cdi/types/Generators.hpp
  src/linglong/cdi/types/helper.hpp
  src/linglong/cdi/types/Hook.hpp
  src/linglong/cdi/types/IntelRdt.hpp
  src/linglong/cdi/types/LinuxNetDevice.hpp
  src/linglong/cdi/types/Mount.hpp
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  fmt::fmt-lib
  linglong::api
  linglong::common
  linglong::utils
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp
  ytj::ytj)
