# SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# The former single pfl_add_library() target has been split into per-subsystem
# static libraries so that a change to one subsystem only rebuilds the affected
# translation units instead of the whole monolithic target. See issue LL-13,
# evaluation items 1 and 3:
#   1. Split the monolithic linglong::linglong target.
#   3. Stop exposing src/ as a PUBLIC include directory (MERGED_HEADER_PLACEMENT)
#      and give each sub-target src/ as a PRIVATE include instead.
#
# linglong::linglong is kept as an INTERFACE library that aggregates the
# sub-targets and re-exposes src/ as an INTERFACE include directory, so all
# existing consumers (apps and tests) continue to build without modification.

# External libraries linked by all linglong sub-targets.
set(_LINGLONG_LINK_LIBS
  rt # for shm_open and shm_unlink
  pthread
  linglong::dbus-api
  linglong::utils
  linglong::api
  PkgConfig::glib2
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
  ${YAML_CPP}
  PkgConfig::uuid)

# Private include directories shared by every sub-target. PRIVATE (not PUBLIC)
# is the whole point of item 3: internal headers under src/ are no longer
# exposed to downstream consumers through the link graph.
set(_LINGLONG_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/src)

# Helper: apply common settings to a linglong sub-target.
function(_linglong_init_target name)
  target_include_directories(${name} PRIVATE ${_LINGLONG_INCLUDE_DIRS})
  target_compile_features(${name} PUBLIC cxx_std_17)
  target_link_libraries(${name} PUBLIC ${_LINGLONG_LINK_LIBS})
endfunction()

# ---------------------------------------------------------------------------
# linglong::linglong_common — common + extension (leaf)
# ---------------------------------------------------------------------------
add_library(linglong__linglong_common STATIC
  src/linglong/common/dbus/register.cpp
  src/linglong/common/dbus/register.h
  src/linglong/common/cli/repo.cpp
  src/linglong/common/cli/repo.h
  src/linglong/common/formatter.cpp
  src/linglong/common/formatter.h
  src/linglong/common/gkeyfile_wrapper.h
  src/linglong/common/global/initialize.cpp
  src/linglong/common/global/initialize.h
  src/linglong/common/serialize/json.cpp
  src/linglong/common/serialize/json.h
  src/linglong/extension/extension.cpp
  src/linglong/extension/extension.h)
add_library(linglong::linglong_common ALIAS linglong__linglong_common)
_linglong_init_target(linglong__linglong_common)

# ---------------------------------------------------------------------------
# linglong::linglong_package
# ---------------------------------------------------------------------------
add_library(linglong__linglong_package STATIC
  src/linglong/package/architecture.cpp
  src/linglong/package/architecture.h
  src/linglong/package/elf_handler.cpp
  src/linglong/package/elf_handler.h
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
  src/linglong/package/versionv2.h)
add_library(linglong::linglong_package ALIAS linglong__linglong_package)
_linglong_init_target(linglong__linglong_package)
target_link_libraries(linglong__linglong_package PUBLIC linglong::linglong_common)

# ---------------------------------------------------------------------------
# linglong::linglong_repo — repo + package_manager + adaptors
# ---------------------------------------------------------------------------
add_library(linglong__linglong_repo STATIC
  src/linglong/adaptors/package_manager/package_manager1.cpp
  src/linglong/adaptors/package_manager/package_manager1.h
  src/linglong/adaptors/task/task1.cpp
  src/linglong/adaptors/task/task1.h
  src/linglong/package_manager/action.cpp
  src/linglong/package_manager/action.h
  src/linglong/package_manager/data_monitor.cpp
  src/linglong/package_manager/data_monitor.h
  src/linglong/package_manager/package_manager.cpp
  src/linglong/package_manager/package_manager.h
  src/linglong/package_manager/package_task.cpp
  src/linglong/package_manager/package_task.h
  src/linglong/package_manager/package_update.cpp
  src/linglong/package_manager/package_update.h
  src/linglong/package_manager/polkit_authority.cpp
  src/linglong/package_manager/polkit_authority.h
  src/linglong/package_manager/ref_installation.cpp
  src/linglong/package_manager/ref_installation.h
  src/linglong/package_manager/task.cpp
  src/linglong/package_manager/task.h
  src/linglong/package_manager/uab_installation.cpp
  src/linglong/package_manager/uab_installation.h
  src/linglong/repo/client_factory.cpp
  src/linglong/repo/client_factory.h
  src/linglong/repo/config.cpp
  src/linglong/repo/config.h
  src/linglong/repo/migrate.cpp
  src/linglong/repo/migrate.h
  src/linglong/repo/ostree_repo.cpp
  src/linglong/repo/ostree_repo.h
  src/linglong/repo/remote_packages.cpp
  src/linglong/repo/remote_packages.h
  src/linglong/repo/repo_cache.cpp
  src/linglong/repo/repo_cache.h)
add_library(linglong::linglong_repo ALIAS linglong__linglong_repo)
_linglong_init_target(linglong__linglong_repo)
target_link_libraries(linglong__linglong_repo PUBLIC linglong::linglong_common linglong::linglong_package)

# ---------------------------------------------------------------------------
# linglong::linglong_runtime — cli + runtime
# ---------------------------------------------------------------------------
add_library(linglong__linglong_runtime STATIC
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
  src/linglong/runtime/container_builder.cpp
  src/linglong/runtime/container_builder.h
  src/linglong/runtime/container.cpp
  src/linglong/runtime/container.h
  src/linglong/runtime/layer.cpp
  src/linglong/runtime/layer.h
  src/linglong/runtime/overlayfs_driver.cpp
  src/linglong/runtime/overlayfs_driver.h
  src/linglong/runtime/run_context.cpp
  src/linglong/runtime/run_context.h
  src/linglong/runtime/security_context.cpp
  src/linglong/runtime/security_context.h)
add_library(linglong::linglong_runtime ALIAS linglong__linglong_runtime)
_linglong_init_target(linglong__linglong_runtime)
target_link_libraries(linglong__linglong_runtime PUBLIC linglong::linglong_common linglong::linglong_package linglong::linglong_repo)

# ---------------------------------------------------------------------------
# linglong::linglong_builder
# ---------------------------------------------------------------------------
add_library(linglong__linglong_builder STATIC
  src/linglong/builder/builder_releases.qrc
  src/linglong/builder/config.cpp
  src/linglong/builder/config.h
  src/linglong/builder/linglong_builder.cpp
  src/linglong/builder/linglong_builder.h
  src/linglong/builder/printer.h
  src/linglong/builder/source_fetcher.cpp
  src/linglong/builder/source_fetcher.h)
add_library(linglong::linglong_builder ALIAS linglong__linglong_builder)
_linglong_init_target(linglong__linglong_builder)
target_link_libraries(linglong__linglong_builder PUBLIC linglong::linglong_common linglong::linglong_package linglong::linglong_repo linglong::linglong_runtime)

# ---------------------------------------------------------------------------
# linglong::linglong — INTERFACE aggregate (backward compatibility)
# ---------------------------------------------------------------------------
add_library(linglong__linglong INTERFACE)
add_library(linglong::linglong ALIAS linglong__linglong)
target_include_directories(linglong__linglong INTERFACE ${_LINGLONG_INCLUDE_DIRS})
target_link_libraries(linglong__linglong INTERFACE
  linglong::linglong_common
  linglong::linglong_package
  linglong::linglong_repo
  linglong::linglong_runtime
  linglong::linglong_builder)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ENABLE_TESTING)
  add_subdirectory(tests/ll-tests)
endif()

# ---------------------------------------------------------------------------
# Wayland security-context support (optional, off by default)
# ---------------------------------------------------------------------------
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

  get_real_target_name(target linglong::linglong_runtime)
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

# ---------------------------------------------------------------------------
# D-Bus adaptors
# ---------------------------------------------------------------------------
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
  linglong::linglong_repo
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  src/linglong/adaptors/package_manager/gen_org_deepin_linglong_packagemanager1
  OrgDeepinLinglongPackagemanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong::linglong_repo
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Task1.xml
  linglong/package_manager/package_task.h
  linglong::service::PackageTask
  src/linglong/adaptors/task/gen_org_deepin_linglong_task1
  OrgDeepinLinglongTask1Adaptor)
