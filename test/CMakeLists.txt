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
    stdc++)

add_executable(ll-test
               "../src/module/util/json.h"
               "../src/module/util/yaml.h"
               "../src/module/runtime/oci.h"
               "../src/module/runtime/container.h"
               "../src/module/package/package.h"
               "../src/module/util/HttpClient.cpp"
               "../src/module/repo/RepoHelper.cpp"
               package_test.cpp
	       #uossv_test.cpp
               extract_test.cpp
#begin add by hqh for debug 2021-9-16
               HttpClient_test.cpp
               RepoHelper_test.cpp
#end add by hqh for debug 2021-9-16
               oci_test.cpp
               qt_json.cpp
               qt_yaml.cpp
               qt_yaml.h)

# begin add by hqh debug 2021-9-16
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
#添加 libcurl库 需要安装 libcurl4-gnutls-dev
set(CURL_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libcurl.so)
#添加库glib和gio库 需要安装libglib2.0-dev
set(GLIB_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libglib-2.0.so /usr/lib/x86_64-linux-gnu/libgio-2.0.so)
set(GOBJ_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libgobject-2.0.so)
#添加ostree 需要安装libostree-dev
set(OSTREE_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libostree-1.so)
#libcurl头文件
set(CURL_INCLUDEDIRS /usr/include/x86_64-linux-gnu/)
#添加头文件目录
set(GLIB_INCLUDEDIRS /usr/include/glib-2.0/ /usr/lib/x86_64-linux-gnu/glib-2.0/include/)
set(OSTREE_INCLUDEDIRS /usr/include/ostree-1/)
set(GOBJ_INCLUDEDIRS /usr/include/glib-2.0/)
endif()
# end add by hqh debug 2021-9-16

target_include_directories(ll-test PRIVATE
#${JSONSTRUCT_INCLUDE_DIR}
${CURL_INCLUDEDIRS}
${GLIB_INCLUDEDIRS}
${OSTREE_INCLUDEDIRS}
${GOBJ_INCLUDEDIRS}
)     
target_link_libraries(ll-test ${LINK_LIBS} -larchive -lcrypto++
${CURL_LIBRARYDIR}
${GLIB_LIBRARYDIR}
${OSTREE_LIBRARYDIR}
${GOBJ_LIBRARYDIR}
)

add_test(NAME ll_test COMMAND ll-test)
