enable_testing()

find_package(GTest REQUIRED)

add_executable(ll-test
               oci-test.cpp)

target_link_libraries(ll-test GTest::GTest gtest_main stdc++)

add_test(NAME ll_test COMMAND ll-test)
