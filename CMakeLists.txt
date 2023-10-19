# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.23)

project(
  linglong
  VERSION 1.4.0
  DESCRIPTION "a container based package manager for deepin"
  HOMEPAGE_URL "https://github.com/linuxdeepin/linglong"
  LANGUAGES CXX)

set(LINGLONG_USERNAME
    "deepin-linglong"
    CACHE STRING "The username for linglong package manager")
set(LINGLONG_ROOT
    "/var/lib/linglong"
    CACHE STRING "The location where linglong related program puts their data")
set(LINGLONG_ENABLE_TESTING
    True
    CACHE BOOL "Enable test of linglong or not")

# Find dependencies

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# NOTE(black_desk): Qt keywords conflict with glib.
add_definitions("-DQT_NO_KEYWORDS")

# NOTE(black_desk): Enable Qt logging with context.
add_definitions("-DQT_MESSAGELOGCONTEXT")

find_package(
  Qt5
  COMPONENTS Core Concurrent DBus Network WebSockets Sql
  REQUIRED)

include(./cmake/CPM.cmake)
include(./cmake/PFL.cmake)

# All dependencies can be added with CPM should be added with CPM.

CPMFindPackage(
  NAME expected
  VERSION 1.0.0
  GITHUB_REPOSITORY TartanLlama/expected
  GIT_TAG v1.0.0
  GIT_SHALLOW ON
  OPTIONS "EXPECTED_BUILD_TESTS OFF"
  FIND_PACKAGE_ARGUMENTS "NAMES tl-expected"
  EXCLUDE_FROM_ALL ON)

if(NOT TARGET tl::expected)
  # NOTE(black_desk): v1.0.0 version of tl::expected doesn't have a namespaced
  # alias so we have to add it here.
  add_library(tl::expected ALIAS expected)
endif()

CPMFindPackage(
  NAME yaml-cpp
  VERSION 0.7.0
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG yaml-cpp-0.7.0
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL ON)

if(NOT TARGET yaml-cpp::yaml-cpp)
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

find_package(PkgConfig REQUIRED)

pkg_search_module(glib2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_search_module(ostree1 REQUIRED IMPORTED_TARGET ostree-1)
pkg_search_module(systemd REQUIRED IMPORTED_TARGET libsystemd)

set(QSERIALIZER_QT_VERSION_MAJOR "5")
pfl_init(ENABLE_TESTING ${LINGLONG_ENABLE_TESTING} EXTERNALS qserializer
         INSTALL ON)

if(LINGLONG_ENABLE_TESTING)
  enable_testing()
endif()

include(GNUInstallDirs)

pfl_add_library(
  INTERNAL
  VERSION
  ${PROJECT_VERSION}
  INS
  ./src/linglong/util/configure.h.in
  SOURCES
  ./src/linglong/adaptors/app_manager/app_manager1.cpp
  ./src/linglong/adaptors/app_manager/app_manager1.h
  ./src/linglong/adaptors/job_manager/job1.cpp
  ./src/linglong/adaptors/job_manager/job1.h
  ./src/linglong/adaptors/job_manager/job_manager1.cpp
  ./src/linglong/adaptors/job_manager/job_manager1.h
  ./src/linglong/adaptors/package_manager/package_manager1.cpp
  ./src/linglong/adaptors/package_manager/package_manager1.h
  ./src/linglong/adaptors/system_helper/filesystem_helper1.cpp
  ./src/linglong/adaptors/system_helper/filesystem_helper1.h
  ./src/linglong/adaptors/system_helper/package_manager_helper1.cpp
  ./src/linglong/adaptors/system_helper/package_manager_helper1.h
  ./src/linglong/api/v1/dbus/app_manager1.cpp
  ./src/linglong/api/v1/dbus/app_manager1.h
  ./src/linglong/api/v1/dbus/filesystem_helper1.cpp
  ./src/linglong/api/v1/dbus/filesystem_helper1.h
  ./src/linglong/api/v1/dbus/package_manager1.cpp
  ./src/linglong/api/v1/dbus/package_manager1.h
  ./src/linglong/api/v1/dbus/package_manager_helper1.cpp
  ./src/linglong/api/v1/dbus/package_manager_helper1.h
  ./src/linglong/api/v1/types/CommonOptions.cpp
  ./src/linglong/api/v1/types/CommonOptions.h
  ./src/linglong/api/v1/types/CommonReply.cpp
  ./src/linglong/api/v1/types/CommonReply.h
  ./src/linglong/api/v1/types/DBusProxyOptions.cpp
  ./src/linglong/api/v1/types/DBusProxyOptions.h
  ./src/linglong/api/v1/types/ExecuteOptions.cpp
  ./src/linglong/api/v1/types/ExecuteOptions.h
  ./src/linglong/api/v1/types/StartOptions.cpp
  ./src/linglong/api/v1/types/StartOptions.h
  ./src/linglong/builder/builder_templates.qrc
  ./src/linglong/builder/builder.h
  ./src/linglong/builder/builder_config.cpp
  ./src/linglong/builder/builder_config.h
  ./src/linglong/builder/depend_fetcher.cpp
  ./src/linglong/builder/depend_fetcher.h
  ./src/linglong/builder/linglong_builder.cpp
  ./src/linglong/builder/linglong_builder.h
  ./src/linglong/builder/project.cpp
  ./src/linglong/builder/project.h
  ./src/linglong/builder/source_fetcher.cpp
  ./src/linglong/builder/source_fetcher.h
  ./src/linglong/builder/source_fetcher_p.h
  ./src/linglong/dbus_ipc/dbus_common.cpp
  ./src/linglong/dbus_ipc/dbus_common.h
  ./src/linglong/dbus_ipc/dbus_system_helper_common.cpp
  ./src/linglong/dbus_ipc/dbus_system_helper_common.h
  ./src/linglong/dbus_ipc/package_manager_param.cpp
  ./src/linglong/dbus_ipc/package_manager_param.h
  ./src/linglong/dbus_ipc/param_option.cpp
  ./src/linglong/dbus_ipc/param_option.h
  ./src/linglong/dbus_ipc/reply.cpp
  ./src/linglong/dbus_ipc/reply.h
  ./src/linglong/dbus_ipc/workaround.cpp
  ./src/linglong/dbus_ipc/workaround.h
  ./src/linglong/job_manager/job.cpp
  ./src/linglong/job_manager/job.h
  ./src/linglong/job_manager/job_manager.cpp
  ./src/linglong/job_manager/job_manager.h
  ./src/linglong/package/bundle.cpp
  ./src/linglong/package/bundle.h
  ./src/linglong/package/info.cpp
  ./src/linglong/package/info.h
  ./src/linglong/package/package.cpp
  ./src/linglong/package/package.h
  ./src/linglong/package/ref.cpp
  ./src/linglong/package/ref.h
  ./src/linglong/package_manager/package_manager.cpp
  ./src/linglong/package_manager/package_manager.h
  ./src/linglong/repo/ostree_repo.cpp
  ./src/linglong/repo/ostree_repo.h
  ./src/linglong/repo/ostree_repohelper.cpp
  ./src/linglong/repo/ostree_repohelper.h
  ./src/linglong/repo/repo.cpp
  ./src/linglong/repo/repo.h
  ./src/linglong/repo/repo_client.cpp
  ./src/linglong/repo/repo_client.h
  ./src/linglong/repo/vfs_repo.cpp
  ./src/linglong/repo/vfs_repo.h
  ./src/linglong/runtime/app.cpp
  ./src/linglong/runtime/resource/app_configs.qrc
  ./src/linglong/runtime/app.h
  ./src/linglong/runtime/app_config.cpp
  ./src/linglong/runtime/app_config.h
  ./src/linglong/runtime/container.cpp
  ./src/linglong/runtime/container.h
  ./src/linglong/runtime/oci.cpp
  ./src/linglong/runtime/oci.h
  ./src/linglong/service/app_manager.cpp
  ./src/linglong/service/app_manager.h
  ./src/linglong/system_helper/filesystem_helper.cpp
  ./src/linglong/system_helper/filesystem_helper.h
  ./src/linglong/system_helper/package_manager_helper.cpp
  ./src/linglong/system_helper/package_manager_helper.h
  ./src/linglong/system_helper/privilege/privilege_install_portal.cpp
  ./src/linglong/system_helper/privilege/privilege_install_portal.h
  ./src/linglong/util/app_status.cpp
  ./src/linglong/util/app_status.h
  ./src/linglong/util/appinfo_cache.cpp
  ./src/linglong/util/appinfo_cache.h
  ./src/linglong/util/command_helper.cpp
  ./src/linglong/util/command_helper.h
  ./src/linglong/util/config/config.cpp
  ./src/linglong/util/config/config.h
  ./src/linglong/util/config/repo.cpp
  ./src/linglong/util/config/repo.h
  ./src/linglong/util/configure.cpp
  ./src/linglong/util/connection.cpp
  ./src/linglong/util/connection.h
  ./src/linglong/util/env.cpp
  ./src/linglong/util/env.h
  ./src/linglong/util/erofs.cpp
  ./src/linglong/util/erofs.h
  ./src/linglong/util/error.cpp
  ./src/linglong/util/error.h
  ./src/linglong/util/file.cpp
  ./src/linglong/util/file.h
  ./src/linglong/util/http/http_client.cpp
  ./src/linglong/util/http/http_client.h
  ./src/linglong/util/oci/distribution_client.cpp
  ./src/linglong/util/oci/distribution_client.h
  ./src/linglong/util/qserializer/dbus.cpp
  ./src/linglong/util/qserializer/dbus.h
  ./src/linglong/util/qserializer/deprecated.cpp
  ./src/linglong/util/qserializer/deprecated.h
  ./src/linglong/util/qserializer/json.cpp
  ./src/linglong/util/qserializer/json.h
  ./src/linglong/util/qserializer/qserializer.cpp
  ./src/linglong/util/qserializer/qserializer.h
  ./src/linglong/util/qserializer/yaml.cpp
  ./src/linglong/util/qserializer/yaml.h
  ./src/linglong/util/runner.cpp
  ./src/linglong/util/runner.h
  ./src/linglong/util/singleton.cpp
  ./src/linglong/util/singleton.h
  ./src/linglong/util/status_code.cpp
  ./src/linglong/util/status_code.h
  ./src/linglong/util/sysinfo.cpp
  ./src/linglong/util/sysinfo.h
  ./src/linglong/util/test/tool.cpp
  ./src/linglong/util/test/tool.h
  ./src/linglong/util/uuid.cpp
  ./src/linglong/util/uuid.h
  ./src/linglong/util/version/semver.cpp
  ./src/linglong/util/version/semver.h
  ./src/linglong/util/version/version.cpp
  ./src/linglong/util/version/version.h
  ./src/linglong/util/xdg.cpp
  ./src/linglong/util/xdg.h
  ./src/linglong/utils/dbus/log.cpp
  ./src/linglong/utils/dbus/log.h
  ./src/linglong/utils/dbus/register.cpp
  ./src/linglong/utils/dbus/register.h
  ./src/linglong/utils/error/error.cpp
  ./src/linglong/utils/error/error.h
  ./src/linglong/utils/finally/finally.cpp
  ./src/linglong/utils/finally/finally.h
  ./src/linglong/utils/global/initialize.cpp
  ./src/linglong/utils/global/initialize.h
  ./src/linglong/utils/serialize/json.cpp
  ./src/linglong/utils/serialize/json.h
  ./src/linglong/utils/serialize/yaml.cpp
  ./src/linglong/utils/serialize/yaml.h
  ./src/linglong/utils/xdg/desktop_entry.h
  ./src/linglong/utils/xdg/desktop_entry.cpp
  APPS
  ll-builder
  ll-cli
  ll-package-manager
  ll-service
  ll-system-helper
  llpkg
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  PkgConfig::glib2
  PkgConfig::ostree1
  PkgConfig::systemd
  QSerializer::DBus
  Qt::Concurrent
  Qt::Core
  Qt::DBus
  Qt::Network
  Qt::Sql
  Qt::WebSockets
  tl::expected
  tl::expected
  yaml-cpp::yaml-cpp)

function(
  linglong_add_dbus_adaptor
  target_name
  xml
  includes
  parent_class
  basename
  classname)
  set(DBUS_ADAPTOR_SOURCE)
  qt5_add_dbus_adaptor(DBUS_ADAPTOR_SOURCE ${xml} ${includes} ${parent_class}
                       ${basename} ${classname})
  # FIXME(black_desk): `qt5_add_dbus_adaptor` will use basename as include and
  # this cannot be configured at all. This is just a workaround.
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_sources(${target_name} PRIVATE ${DBUS_ADAPTOR_SOURCE})
endfunction()

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager1.xml
  linglong/service/app_manager.h
  linglong::service::AppManager
  src/linglong/adaptors/app_manager/gen_org_deepin_linglong_appmanager1
  OrgDeepinLinglongAppmanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Job1.xml
  linglong/job_manager/job.h
  linglong::job_manager::Job
  src/linglong/adaptors/job_manager/gen_org_deepin_linglong_job1
  OrgDeepinLinglongJob1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.JobManager1.xml
  linglong/job_manager/job_manager.h
  linglong::job_manager::JobManager
  src/linglong/adaptors/job_manager/gen_org_deepin_linglong_jobmanager1
  OrgDeepinLinglongJobmanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.JobManager1.xml
  linglong/job_manager/job_manager.h
  linglong::job_manager::JobManager
  src/linglong/adaptors/job_manager/gen_org_deepin_linglong_jobmanager1
  OrgDeepinLinglongJobmanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  src/linglong/adaptors/package_manager/gen_org_deepin_linglong_packagemanager1
  OrgDeepinLinglongPackagemanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  src/linglong/adaptors/package_manager/gen_org_deepin_linglong_packagemanager1
  OrgDeepinLinglongPackagemanager1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper1.xml
  linglong/system_helper/filesystem_helper.h
  linglong::system::helper::FilesystemHelper # FIXME: should be
                                             # linglong::system_helper::FilesystemHelper
  src/linglong/adaptors/system_helper/gen_org_deepin_linglong_filesystemhelper1
  OrgDeepinLinglongFilesystemHelper1Adaptor)

linglong_add_dbus_adaptor(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper1.xml
  linglong/system_helper/package_manager_helper.h
  linglong::system::helper::PackageManagerHelper # FIXME: should be
                                                 # linglong::system_helper::PackageManagerHelper
  src/linglong/adaptors/system_helper/gen_org_deepin_linglong_packagemanagerhelper1
  OrgDeepinLinglongPackageManagerHelper1Adaptor)

function(linglong_add_dbus_interface target xml basename) # include
  set_source_files_properties(${xml} NO_NAMESPACE ON)

  set(include "${ARGV3}")
  if(include)
    set_source_files_properties(${xml} PROPERTIES INCLUDE ${include})
  endif()

  set(INTERFACE_SOURCES)
  qt5_add_dbus_interface(INTERFACE_SOURCES ${xml} ${basename})
  target_sources(${target} PRIVATE ${INTERFACE_SOURCES})
endfunction()

linglong_add_dbus_interface(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper1.xml
  src/linglong/api/v1/dbus/gen_org_deepin_linglong_packagemanagerhelper1)

linglong_add_dbus_interface(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper1.xml
  src/linglong/api/v1/dbus/gen_org_deepin_linglong_filesystemhelper1
  linglong/dbus_ipc/workaround.h)

linglong_add_dbus_interface(
  linglong ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager1.xml
  src/linglong/api/v1/dbus/gen_org_deepin_linglong_appmanager1
  linglong/dbus_ipc/workaround.h)

linglong_add_dbus_interface(
  linglong
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  src/linglong/api/v1/dbus/gen_org_deepin_linglong_packagemanager1
  linglong/dbus_ipc/workaround.h)

add_subdirectory(misc)
