enable_testing()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(GTest REQUIRED)
find_package(
  Qt5 REQUIRED
  COMPONENTS Core
             DBus
             Sql
             Network
)
find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(
  OSTREE1
  REQUIRED
  ostree-1
)

set(Qt_LIBS
    Qt5::Core
    Qt5::DBus
    Qt5::Sql
    Qt5::Network
)

set(LINK_LIBS
    ${Qt_LIBS}
    yaml-cpp
    GTest::GTest
    gtest_main
    stdc++
    CURL::libcurl
    ${OSTREE1_LDFLAGS}
)

set(LL_TEST_SOURCES
    # cmake-format: sortable
    ../src/module/flatpak/flatpak_manager.cpp
    ../src/module/package/info.cpp
    ../src/module/package/info.h
    ../src/module/package/package.cpp
    ../src/module/package/package.h
    ../src/module/repo/ostree_repohelper.cpp
    ../src/module/runtime/container.cpp
    ../src/module/runtime/oci.h
    ../src/module/util/connection.cpp
    ../src/module/util/desktop_entry.cpp
    ../src/module/util/file.cpp
    ../src/module/repo/repo_client.cpp
    ../src/module/repo/repo_client.h
    ../src/module/runtime/container.cpp
    ../src/module/runtime/oci.h
    ../src/module/util/config/config.h
    ../src/module/util/connection.cpp
    ../src/module/util/desktop_entry.cpp
    ../src/module/util/file.cpp
    ../src/module/util/http/http_client.cpp
    ../src/module/util/http/http_client.h
    ../src/module/util/httpclient.cpp
    ../src/module/util/json.h
    ../src/module/util/sysinfo.cpp
    ../src/module/util/uuid.cpp
    ../src/module/util/xdg.cpp
    ../src/service/impl/param_option.h
    ../src/service/impl/reply.h
    ../src/system/impl/app_status.cpp
    ../src/system/impl/appinfo_cache.cpp
    module/package/package_info_test.cpp
    module/package/ref_test.cpp
    module/util/app_status_test.cpp
    module/util/fs_test.cpp
    module/util/httpclient_test.cpp
    module/util/result_test.cpp
    module/util/runner_test.cpp
    module/util/uuid_test.cpp
    module/util/xdg_test.cpp
    module/serialize/serialize_test.h
    module/serialize/serialize_test.cpp
    package_manager_test.cpp
    module/serialize/qt_json.cpp
    module/serialize/qt_yaml.cpp
    module/config/config_test.cpp
    repohelper_test.cpp
)

# add qdbus call
set_source_files_properties(
  ../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE service/impl/register_meta_type.h
)

qt5_add_dbus_interface(
  LL_TEST_SOURCES
  ../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
  package_manager
)

set_source_files_properties(
  ../src/module/dbus_ipc/com.deepin.linglong.SystemPackageManager.xml
  PROPERTIES INCLUDE service/impl/register_meta_type.h
)

qt5_add_dbus_interface(
  LL_TEST_SOURCES
  ../src/module/dbus_ipc/com.deepin.linglong.SystemPackageManager.xml
  system_package_manager
)

add_executable(
  ll-test
  ${LL_TEST_SOURCES}
)

target_include_directories(
  ll-test
  PRIVATE ${OSTREE1_INCLUDE_DIRS}
  PRIVATE ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ll-test PRIVATE ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
