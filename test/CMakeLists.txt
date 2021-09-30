enable_testing()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(GTest REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(yaml-cpp REQUIRED)

set(Qt_LIBS Qt5::Core Qt5::DBus)

set(LINK_LIBS
		${Qt_LIBS}
		yaml-cpp
		GTest::GTest gtest_main
		stdc++
		-larchive -lcrypto++
		CURL::libcurl
		${OSTREE1_LDFLAGS}
		${APPSTREMGLIB_LDFLAGS}
		)

add_executable(ll-test
               "../src/module/util/json.h"
               "../src/module/util/yaml.h"
               "../src/module/runtime/oci.h"
               "../src/module/runtime/container.h"
               "../src/module/package/package.h"
               "../src/module/util/httpclient.cpp"
               "../src/module/repo/repohelper.cpp"
               package_test.cpp
	           # uossv_test.cpp
               extract_test.cpp
# test mod util fs
               util_fs_test.cpp
#begin add by hqh for debug 2021-9-16
               httpclient_test.cpp
               repohelper_test.cpp
#end add by hqh for debug 2021-9-16
               oci_test.cpp
               qt_json.cpp
               qt_yaml.cpp
               qt_yaml.h
	       get_info_test.cpp
	       build_ouap_test.cpp
	       )

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)
pkg_search_module(APPSTREMGLIB REQUIRED appstream-glib)

target_include_directories(ll-test PRIVATE
		${OSTREE1_INCLUDE_DIRS}
		${APPSTREMGLIB_INCLUDE_DIRS}
)

target_link_libraries(ll-test ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
