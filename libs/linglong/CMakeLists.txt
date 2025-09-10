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
  src/linglong/adaptors/task/task1.cpp
  src/linglong/adaptors/task/task1.h
  src/linglong/builder/config.cpp
  src/linglong/builder/config.h
  src/linglong/builder/linglong_builder.cpp
  src/linglong/builder/linglong_builder.h
  src/linglong/builder/printer.h
  src/linglong/builder/source_fetcher.cpp
  src/linglong/builder/source_fetcher.h
  src/linglong/cli/cli.cpp
  src/linglong/cli/cli.h
  src/linglong/cli/cli_printer.cpp
  src/linglong/cli/cli_printer.h
  src/linglong/cli/dbus_notifier.cpp
  src/linglong/cli/dbus_notifier.h
  src/linglong/cli/dummy_notifier.cpp
  src/linglong/cli/dummy_notifier.h
  src/linglong/cli/interactive_notifier.h
  src/linglong/cli/json_printer.cpp
  src/linglong/cli/json_printer.h
  src/linglong/cli/printer.h
  src/linglong/cli/terminal_notifier.cpp
  src/linglong/cli/terminal_notifier.h
  src/linglong/extension/extension.cpp
  src/linglong/extension/extension.h
  src/linglong/package/architecture.cpp
  src/linglong/package/architecture.h
  src/linglong/package/fallback_version.cpp
  src/linglong/package/fallback_version.h
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
  src/linglong/package_manager/package_task.cpp
  src/linglong/package_manager/package_task.h
  src/linglong/package/reference.cpp
  src/linglong/package/reference.h
  src/linglong/package/semver.hpp
  src/linglong/package/uab_file.cpp
  src/linglong/package/uab_file.h
  src/linglong/package/uab_packager.cpp
  src/linglong/package/uab_packager.h
  src/linglong/package/version.cpp
  src/linglong/package/version.h
  src/linglong/package/versionv1.cpp
  src/linglong/package/versionv1.h
  src/linglong/package/versionv2.cpp
  src/linglong/package/versionv2.h
  src/linglong/repo/client_factory.cpp
  src/linglong/repo/client_factory.h
  src/linglong/repo/config.cpp
  src/linglong/repo/config.h
  src/linglong/repo/migrate.cpp
  src/linglong/repo/migrate.h
  src/linglong/repo/ostree_repo.cpp
  src/linglong/repo/ostree_repo.h
  src/linglong/repo/repo_cache.cpp
  src/linglong/repo/repo_cache.h
  src/linglong/runtime/container_builder.cpp
  src/linglong/runtime/container_builder.h
  src/linglong/runtime/container.cpp
  src/linglong/runtime/container.h
  src/linglong/runtime/run_context.cpp
  src/linglong/runtime/run_context.h
  src/linglong/runtime/security_context.cpp
  src/linglong/runtime/security_context.h
  TESTS
  ll-tests
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  rt # for shm_open and shm_unlink
  pthread
  linglong::dbus-api
  linglong::utils
  linglong::api
  PkgConfig::ostree1
  PkgConfig::systemd
  PkgConfig::ELF
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::DBus
  LinglongRepoClientAPI
  ytj::ytj
  tl::expected
  linglong::oci-cfg-generators
  CLI11::CLI11
  ${YAML_CPP})

if(LINGLONG_ENABLE_WAYLAND_SEC_CTX_SUPPORT)
  # pkg_get_variable() cannot reliably get variables in a .pc file if those
  # variables contain placeholders/macros (like ${pc_sysrootdir}) that CMake
  # cannot expand.
  execute_process(
    COMMAND pkg-config --variable=pkgdatadir wayland-protocols
    OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DATA_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PKG_CONFIG_RESULT)

  if(NOT PKG_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to get pkgdatadir from wayland-protocols")
  endif()

  set(WAYLAND_SECURITY_CONTEXT_XML
      ${WAYLAND_PROTOCOLS_DATA_DIR}/staging/security-context/security-context-v1.xml
  )
  set(WAYLAND_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland)
  set(WAYLAND_SECURITY_CONTEXT_HEADER
      ${WAYLAND_GENERATED_DIR}/wayland-security-context-v1.h)
  set(WAYLAND_SECURITY_CONTEXT_GENERATED_FILES
      ${WAYLAND_GENERATED_DIR}/wayland-security-context-v1.c)

  add_custom_command(
    OUTPUT ${WAYLAND_SECURITY_CONTEXT_HEADER}
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_SECURITY_CONTEXT_XML}
            ${WAYLAND_SECURITY_CONTEXT_HEADER}
    DEPENDS ${WAYLAND_SECURITY_CONTEXT_XML})

  add_custom_command(
    OUTPUT ${WAYLAND_SECURITY_CONTEXT_GENERATED_FILES}
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_SECURITY_CONTEXT_XML}
            ${WAYLAND_SECURITY_CONTEXT_GENERATED_FILES}
    DEPENDS ${WAYLAND_SECURITY_CONTEXT_XML})

  get_real_target_name(target linglong::linglong)
  target_include_directories(${target} PRIVATE ${WAYLAND_GENERATED_DIR})
  target_sources(
    ${target}
    PRIVATE
      ${WAYLAND_SECURITY_CONTEXT_GENERATED_FILES}
      ${CMAKE_CURRENT_LIST_DIR}/src/linglong/runtime/wayland_security_ctx.cpp
      ${CMAKE_CURRENT_LIST_DIR}/src/linglong/runtime/wayland_security_ctx.h)
  target_link_libraries(${target} PUBLIC PkgConfig::WAYLAND_CLIENT)
  target_compile_options(${target} PRIVATE -DWAYLAND_SEC_CTX_SUPPORT)
endif()

function(
  linglong_add_dbus_adaptor
  target
  xml
  includes
  parent_class
  basename
  classname)
  get_real_target_name(target ${target})

  if("${QT_VERSION_MAJOR}" STREQUAL "6")
    qt6_add_dbus_adaptor(DBUS_ADAPTOR_SOURCE ${xml} ${includes} ${parent_class}
                         ${basename} ${classname})
  else()
    qt5_add_dbus_adaptor(DBUS_ADAPTOR_SOURCE ${xml} ${includes} ${parent_class}
                         ${basename} ${classname})
  endif()
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
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Task1.xml
  linglong/package_manager/package_task.h
  linglong::service::PackageTask
  src/linglong/adaptors/task/gen_org_deepin_linglong_task1
  OrgDeepinLinglongTask1Adaptor)
