find_package(GTest REQUIRED)
include(GoogleTest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(
  ll-test
  ./module/config/config_test.cpp
  ./module/package/package_info_test.cpp
  ./module/package/ref_test.cpp
  ./module/qserializer/manifest_test.cpp
  ./module/qserializer/object.cpp
  ./module/qserializer/object.h
  ./module/qserializer/test.cpp
  ./module/runtime/app_test.cpp
  ./module/util/error_test.cpp
  ./module/util/fs_test.cpp
  ./module/util/http_client_test.cpp
  ./module/util/oci/distribution_test.cpp
  ./module/util/runner_test.cpp
  ./module/util/uuid_test.cpp
  ./module/util/xdg_test.cpp
)

target_link_libraries(
  ll-test
  PRIVATE GTest::Main
          linglong_lib
          linglong_utils
)

gtest_discover_tests(ll-test WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
