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
  src/linglong/adaptors/migrate/migrate1.cpp
  src/linglong/adaptors/migrate/migrate1.h
  src/linglong/adaptors/package_manager/package_manager1.cpp
  src/linglong/adaptors/package_manager/package_manager1.h
  src/linglong/builder/config.cpp
  src/linglong/builder/config.h
  src/linglong/builder/file.cpp
  src/linglong/builder/file.h
  src/linglong/builder/linglong_builder.cpp
  src/linglong/builder/linglong_builder.h
  src/linglong/builder/printer.h
  src/linglong/builder/source_fetcher.cpp
  src/linglong/builder/source_fetcher.h
  src/linglong/cli/cli.cpp
  src/linglong/cli/cli.h
  src/linglong/cli/dbus_notifier.cpp
  src/linglong/cli/dbus_notifier.h
  src/linglong/cli/dummy_notifier.cpp
  src/linglong/cli/dummy_notifier.h
  src/linglong/cli/interactive_notifier.h
  src/linglong/cli/json_printer.cpp
  src/linglong/cli/json_printer.h
  src/linglong/cli/printer.cpp
  src/linglong/cli/printer.h
  src/linglong/cli/terminal_notifier.cpp
  src/linglong/cli/terminal_notifier.h
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
  src/linglong/package_manager/migrate.cpp
  src/linglong/package_manager/migrate.h
  src/linglong/package_manager/package_manager.cpp
  src/linglong/package_manager/package_manager.h
  src/linglong/package_manager/task.cpp
  src/linglong/package_manager/task.h
  src/linglong/package/reference.cpp
  src/linglong/package/reference.h
  src/linglong/package/uab_file.cpp
  src/linglong/package/uab_file.h
  src/linglong/package/uab_packager.cpp
  src/linglong/package/uab_packager.h
  src/linglong/package/version.cpp
  src/linglong/package/version.h
  src/linglong/package/version_range.cpp
  src/linglong/package/version_range.h
  src/linglong/repo/client_factory.cpp
  src/linglong/repo/client_factory.h
  src/linglong/repo/config.cpp
  src/linglong/repo/config.h
  src/linglong/repo/ostree_repo.cpp
  src/linglong/repo/ostree_repo.h
  src/linglong/repo/repo_cache.cpp
  src/linglong/repo/repo_cache.h
  src/linglong/runtime/container_builder.cpp
  src/linglong/runtime/container_builder.h
  src/linglong/runtime/container.cpp
  src/linglong/runtime/container.h
  # FIXME(black_desk): After refactory, all tests are failed to compile as I
  # have no time to fix them now. Let's bring them back later. TESTS ll-tests
  # http-client-tests
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  linglong::dbus-api
  linglong::utils
  linglong::api
  PkgConfig::ostree1
  PkgConfig::systemd
  PkgConfig::ELF
  Qt5::Concurrent
  Qt5::Core
  Qt5::DBus
  Qt5::Network
  Qt5::WebSockets
  QtLinglongRepoClientAPI
  docopt
  ytj::ytj
  tl::expected
  linglong::ocppi
  ${YAML_CPP})

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

linglong_add_dbus_adaptor(
  linglong::linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Migrate1.xml
  linglong/package_manager/migrate.h
  linglong::service::Migrate
  src/linglong/adaptors/migrate/gen_org_deepin_linglong_migrate1
  OrgDeepinLinglongMigrate1Adaptor)
