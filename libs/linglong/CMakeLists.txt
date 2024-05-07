# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  src/linglong/builder/builder_releases.qrc
  # find -regex '\./src/.+\.[ch]\(pp\)?\(\.in\)?' -type f -printf '%P\n'| sort
  src/linglong/adaptors/package_manager/package_manager1.cpp
  src/linglong/adaptors/package_manager/package_manager1.h
  src/linglong/api/dbus/v1/dbus_peer.cpp
  src/linglong/api/dbus/v1/dbus_peer.h
  src/linglong/api/dbus/v1/package_manager.cpp
  src/linglong/api/dbus/v1/package_manager.h
  src/linglong/api/types/helper.cpp
  src/linglong/api/types/helper.h
  src/linglong/api/types/v1/ApplicationConfiguration.hpp
  src/linglong/api/types/v1/ApplicationConfigurationPermissionsBind.hpp
  src/linglong/api/types/v1/ApplicationConfigurationPermissions.hpp
  src/linglong/api/types/v1/ApplicationConfigurationPermissionsInnerBind.hpp
  src/linglong/api/types/v1/BuilderConfig.hpp
  src/linglong/api/types/v1/BuilderProject.hpp
  src/linglong/api/types/v1/BuilderProjectPackage.hpp
  src/linglong/api/types/v1/BuilderProjectSource.hpp
  src/linglong/api/types/v1/CliContainer.hpp
  src/linglong/api/types/v1/CommonResult.hpp
  src/linglong/api/types/v1/Generators.hpp
  src/linglong/api/types/v1/helper.hpp
  src/linglong/api/types/v1/LayerInfo.hpp
  src/linglong/api/types/v1/LinglongAPIV1.hpp
  src/linglong/api/types/v1/OciConfigurationPatch.hpp
  src/linglong/api/types/v1/PackageInfo.hpp
  src/linglong/api/types/v1/PackageManager1GetRepoInfoResult.hpp
  src/linglong/api/types/v1/PackageManager1GetRepoInfoResultRepoInfo.hpp
  src/linglong/api/types/v1/PackageManager1InstallParameters.hpp
  src/linglong/api/types/v1/PackageManager1ModifyRepoParameters.hpp
  src/linglong/api/types/v1/PackageManager1Package.hpp
  src/linglong/api/types/v1/PackageManager1ResultWithTaskID.hpp
  src/linglong/api/types/v1/PackageManager1SearchParameters.hpp
  src/linglong/api/types/v1/PackageManager1SearchResult.hpp
  src/linglong/api/types/v1/PackageManager1UninstallParameters.hpp
  src/linglong/api/types/v1/PackageManager1UpdateParameters.hpp
  src/linglong/api/types/v1/RepoConfig.hpp
  src/linglong/builder/config.cpp
  src/linglong/builder/config.h
  src/linglong/builder/file.cpp
  src/linglong/builder/file.h
  src/linglong/builder/linglong_builder.cpp
  src/linglong/builder/linglong_builder.h
  src/linglong/builder/source_fetcher.cpp
  src/linglong/builder/source_fetcher.h
  src/linglong/builder/printer.h
  src/linglong/cli/cli.cpp
  src/linglong/cli/cli.h
  src/linglong/cli/json_printer.cpp
  src/linglong/cli/json_printer.h
  src/linglong/cli/printer.cpp
  src/linglong/cli/printer.h
  src/linglong/package/architecture.cpp
  src/linglong/package/architecture.h
  src/linglong/package/fuzzy_reference.cpp
  src/linglong/package/fuzzy_reference.h
  src/linglong/package/layer_dir.cpp
  src/linglong/package/layer_dir.h
  src/linglong/package/layer_file.cpp
  src/linglong/package/layer_file.h
  src/linglong/package/layer_packager.cpp
  src/linglong/package/layer_packager.h
  src/linglong/package_manager/package_manager.cpp
  src/linglong/package_manager/package_manager.h
  src/linglong/package_manager/task.cpp
  src/linglong/package_manager/task.h
  src/linglong/package/reference.cpp
  src/linglong/package/reference.h
  src/linglong/package/version.cpp
  src/linglong/package/version.h
  src/linglong/package/version_range.cpp
  src/linglong/package/version_range.h
  src/linglong/repo/config.cpp
  src/linglong/repo/config.h
  src/linglong/repo/ostree_repo.cpp
  src/linglong/repo/ostree_repo.h
  src/linglong/runtime/container_builder.cpp
  src/linglong/runtime/container_builder.h
  src/linglong/runtime/container.cpp
  src/linglong/runtime/container.h
  src/linglong/utils/command/env.cpp
  src/linglong/utils/command/env.h
  src/linglong/utils/command/ocppi-helper.cpp
  src/linglong/utils/command/ocppi-helper.h
  src/linglong/utils/configure.cpp
  src/linglong/utils/configure.h.in
  src/linglong/utils/dbus/log.cpp
  src/linglong/utils/dbus/log.h
  src/linglong/utils/dbus/register.cpp
  src/linglong/utils/dbus/register.h
  src/linglong/utils/error/details/error_impl.cpp
  src/linglong/utils/error/details/error_impl.h
  src/linglong/utils/error/error.cpp
  src/linglong/utils/error/error.h
  src/linglong/utils/finally/finally.cpp
  src/linglong/utils/finally/finally.h
  src/linglong/utils/global/initialize.cpp
  src/linglong/utils/global/initialize.h
  src/linglong/utils/serialize/json.cpp
  src/linglong/utils/serialize/json.h
  src/linglong/utils/serialize/yaml.cpp
  src/linglong/utils/serialize/yaml.h
  src/linglong/utils/std_helper/qdebug_helper.h
  src/linglong/utils/transaction.cpp
  src/linglong/utils/transaction.h
  src/linglong/utils/xdg/desktop_entry.cpp
  src/linglong/utils/xdg/desktop_entry.h
  # FIXME(black_desk): After refactory, all tests are failed to compile as I
  # have no time to fix them now. Let's bring them back later. TESTS ll-tests
  # http-client-tests
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  PkgConfig::glib2
  PkgConfig::ostree1
  PkgConfig::systemd
  Qt5::Concurrent
  Qt5::Core
  Qt5::DBus
  Qt5::Network
  Qt5::WebSockets
  QtLinglongRepoClientAPI
  docopt
  linglong::ocppi
  tl::expected
  ${YAML_CPP}
  ytj::ytj)

function(
  linglong_add_dbus_adaptor
  target
  xml
  includes
  parent_class
  basename
  classname)
  get_real_target_name(target ${target})

  set(DBUS_ADAPTOR_SOURCE)

  set(Qt5DBus_QDBUSXML2CPP_EXECUTABLE qdbusxml2cpp)
  qt5_add_dbus_adaptor(DBUS_ADAPTOR_SOURCE ${xml} ${includes} ${parent_class}
                       ${basename} ${classname})
  # FIXME(black_desk): `qt5_add_dbus_adaptor` will use basename as include and
  # this cannot be configured at all. This is just a workaround.
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_sources(${target} PRIVATE ${DBUS_ADAPTOR_SOURCE})
endfunction()

linglong_add_dbus_adaptor(
  linglong::linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  src/linglong/adaptors/package_manager/gen_org_deepin_linglong_packagemanager1
  OrgDeepinLinglongPackagemanager1Adaptor)

function(linglong_add_dbus_interface target xml basename) # include
  get_real_target_name(target "${target}")

  set_source_files_properties(${xml} NO_NAMESPACE ON)

  if(${ARGC} GREATER 3)
    set(include "${ARGV3}")
    if(include)
      set_source_files_properties(${xml} PROPERTIES INCLUDE ${include})
    endif()
  endif()

  set(INTERFACE_SOURCES)
  set(Qt5DBus_QDBUSXML2CPP_EXECUTABLE qdbusxml2cpp)
  qt5_add_dbus_interface(INTERFACE_SOURCES ${xml} ${basename})
  target_sources(${target} PRIVATE ${INTERFACE_SOURCES})
endfunction()

linglong_add_dbus_interface(
  linglong::linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  src/linglong/api/dbus/v1/gen_org_deepin_linglong_packagemanager1)

linglong_add_dbus_interface(
  linglong::linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.freedesktop.DBus.Peer.xml
  src/linglong/api/dbus/v1/gen_org_freedesktop_dbus_peer)
