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
               package_test.cpp
	       #uossv_test.cpp
               extract_test.cpp
               oci_test.cpp
               qt_json.cpp
               qt_yaml.cpp
               qt_yaml.h)

       #target_include_directories(ll-test PRIVATE ${JSONSTRUCT_INCLUDE_DIR})
target_link_libraries(ll-test ${LINK_LIBS} -larchive -lcrypto++)

add_test(NAME ll_test COMMAND ll-test)
