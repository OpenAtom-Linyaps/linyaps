enable_testing()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(GTest REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core DBus Sql)
find_package(yaml-cpp REQUIRED)
find_package(DtkCore REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)

set(Qt_LIBS Qt5::Core Qt5::DBus Qt5::Sql)

set(DTK_LIBS dtkcore)

set(LINK_LIBS
    ${Qt_LIBS}
    yaml-cpp
    GTest::GTest gtest_main
    stdc++
    CURL::libcurl
    ${OSTREE1_LDFLAGS})

set(LL_TEST_SOURCES
    ../src/module/util/fs.cpp
    ../src/module/util/xdg.cpp
    ../src/module/package/info.cpp
    ../src/module/util/httpclient.cpp
    ../src/module/repo/ostree_repohelper.cpp
    ../src/module/util/json.h
    ../src/module/runtime/oci.h
    ../src/module/runtime/container.cpp
    ../src/module/util/uuid.cpp
    ../src/module/util/desktop_entry.cpp
    ../src/module/flatpak/flatpak_manager.cpp
    ../src/module/runtime/app.cpp
    ../src/module/package/package.h
    ../src/module/package/info.h
    ../src/module/package/package.cpp
    ../src/service/impl/qdbus_retmsg.h
    ../src/service/impl/package_manager_option.h
    ../src/module/util/sysinfo.cpp
    ../src/module/util/app_status.cpp)

# add qdbus call
set_source_files_properties(../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_TEST_SOURCES
                       ../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                       package_manager)

set_source_files_properties(../src/module/dbus_ipc/com.deepin.linglong.UapManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

add_executable(ll-test
        util_fs_test.cpp
        module/package/ref_test.cpp
        module/util/result_test.cpp
        module/util/xdg_test.cpp
        module/util/fs_test.cpp
        module/package/package_info_test.cpp
        module/runtime/app_test.cpp
        httpclient_test.cpp
        repohelper_test.cpp
        qt_json.cpp
        qt_yaml.cpp
        package_manager_test.cpp
        singleton_test.cpp
        app_status_test.cpp
        ${LL_TEST_SOURCES})

target_include_directories(ll-test
                           PRIVATE ${OSTREE1_INCLUDE_DIRS}
                           PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(ll-test
                      PRIVATE ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
