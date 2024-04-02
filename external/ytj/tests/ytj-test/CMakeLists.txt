# https://github.com/cpm-cmake/CPM.cmake/issues/475
find_package(
  Catch2 2.13.4 QUIET)
if(NOT Catch2_FOUND)
  find_package(Catch2 3.4.0 QUIET)
endif()

if(NOT Catch2_FOUND AND COMMAND CPMFindPackage)
  CPMFindPackage(
    NAME Catch2
    VERSION 2.13.4
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v2.13.5
    EXCLUDE_FROM_ALL ON
    OPTIONS "CATCH_BUILD_STATIC_LIBRARY ON")

  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
endif()

if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found, test will not be built.")
  return()
endif()

pfl_add_executable(
  DISABLE_INSTALL
  OUTPUT_NAME
  ytj_test
  SOURCES
  src/ytj/find_catch2.hpp
  src/ytj/to_json.test.cpp
  src/ytj/to_yaml.test.cpp
  LINK_LIBRARIES
  PRIVATE
  ytj::ytj
  Catch2::Catch2WithMain)

include(Catch)

# NOTE: add_custom_command cannot use with ALIAS target. Check
# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/9201.
get_real_target_name(test ytj::ytj::ytj-test)
catch_discover_tests(${test})
