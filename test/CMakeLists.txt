enable_testing()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(GTest REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)

set(Qt_LIBS Qt5::Core Qt5::DBus)
set(LINK_LIBS
    ${Qt_LIBS}
    GTest::GTest gtest_main
    stdc++)

add_executable(ll-test
               "../src/module/util/json.h"
               "../src/module/runtime/oci.h"
               oci_test.cpp
               qt_json.cpp)

target_link_libraries(ll-test ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
