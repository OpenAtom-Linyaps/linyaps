enable_testing()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(GTest REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core DBus)
find_package(yaml-cpp REQUIRED)
find_package(DtkCore REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)

set(Qt_LIBS Qt5::Core Qt5::DBus)

set(DTK_LIBS dtkcore)

set(LINK_LIBS
    ${Qt_LIBS}
    yaml-cpp
    GTest::GTest gtest_main
    stdc++
    -larchive
    CURL::libcurl
    ${OSTREE1_LDFLAGS})

set(LL_TEST_SOURCES
    ../src/module/util/fs.cpp
    ../src/module/package/info.cpp
    ../src/module/util/httpclient.cpp
    ../src/module/repo/ostree_repohelper.cpp
    ../src/module/util/json.h
    ../src/module/runtime/oci.h
    ../src/module/runtime/container.h
    ../src/module/package/pkginfo.h
    ../src/service/impl/qdbus_retmsg.h)

# add qdbus call
set_source_files_properties(../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_TEST_SOURCES
                       ../src/module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                       package_manager)

set_source_files_properties(../src/module/dbus_ipc/com.deepin.linglong.UapManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_TEST_SOURCES
                       ../src/module/dbus_ipc/com.deepin.linglong.UapManager.xml
                       uap_manager)

add_executable(ll-test
               package_test.cpp
               extract_test.cpp
               util_fs_test.cpp
               module/package/ref_test.cpp
               module/util/result_test.cpp
               httpclient_test.cpp
               repohelper_test.cpp
               qt_json.cpp
               qt_yaml.cpp
               get_info_test.cpp
               build_ouap_test.cpp
               qdbus_retmsg_test.cpp
               qdbus_call_test.cpp
               package_manager_test.cpp
               singleton_test.cpp
               uap_manager_test.cpp
               ${LL_TEST_SOURCES})

target_include_directories(ll-test
                           PRIVATE ${OSTREE1_INCLUDE_DIRS}
                           PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(ll-test
                      PRIVATE ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
