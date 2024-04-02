cmake_minimum_required(VERSION 3.11.4 FATAL_ERROR)

project(
  ytj
  VERSION 0.1.4
  DESCRIPTION
    "A header only cpp library and a command line tool to convert yaml to json."
  HOMEPAGE_URL "https://github.com/black-desk/ytj"
  LANGUAGES CXX)

function(get_real_target_name output target)
  get_target_property("${output}" "${target}" ALIASED_TARGET)
  if("${output}" STREQUAL "")
    set("${output}" "${target}")
  endif()
  set("${output}"
      ${${output}}
      PARENT_SCOPE)
endfunction()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(PFL)
include(GitSemver)
gitsemver(PROJECT_VERSION)

set(YTJ_JSON_VERSION 3.5.0)
set(YTJ_YAML_CPP_VERSION 0.6.2)

if(CMAKE_VERSION VERSION_LESS "3.14")
  message("CPM.cmake disabled.")
else()
  include(CPM)
  CPMFindPackage(
    NAME nlohmann_json
    VERSION ${YTJ_JSON_VERSION}
    URL "https://github.com/nlohmann/json/archive/refs/tags/v3.5.0.tar.gz"
    EXCLUDE_FROM_ALL ON
    OPTIONS "JSON_Install ON" # nlohmann_json is an INTERFACE dependency of ytj
            "JSON_BuildTests OFF")

  CPMFindPackage(
    NAME yaml-cpp
    VERSION ${YTJ_YAML_CPP_VERSION}
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG ab5f925 # NOTE: We need https://github.com/jbeder/yaml-cpp/pull/583
    GIT_PROGRESS ON
    EXCLUDE_FROM_ALL ON
    OPTIONS "YAML_CPP_INSTALL ON" # yaml-cpp is an INTERFACE dependency of ytj
            "YAML_CPP_BUILD_TESTS OFF")
endif()

find_package(
  yaml-cpp ${YTJ_YAML_CPP_VERSION} REQUIRED)

if(NOT TARGET yaml-cpp::yaml-cpp)
  if(CMAKE_VERSION VERSION_LESS 3.18)
    set_target_properties(yaml-cpp PROPERTIES IMPORTED_GLOBAL true)
  endif()
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

find_package(
  nlohmann_json ${YTJ_JSON_VERSION} QUIET)

if(NOT nlohmann_json_FOUND)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.fix)
  find_package(nlohmann_json ${YTJ_JSON_VERSION} REQUIRED)
endif()

include(PFL)

pfl_init()

# build library ytj
pfl_add_library(
  MERGED_HEADER_PLACEMENT
  LIBRARY_TYPE
  HEADER_ONLY
  SOURCES
  src/ytj/ytj.cpp
  src/ytj/ytj.hpp.in
  APPS
  ytj
  TESTS
  ytj-test
  DEPENDENCIES
  PUBLIC
  "yaml-cpp ${YTJ_YAML_CPP_VERSION} REQUIRED"
  "nlohmann_json ${YTJ_JSON_VERSION} REQUIRED"
  COMPILE_FEATURES
  INTERFACE
  cxx_std_17
  LINK_LIBRARIES
  INTERFACE
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp
  stdc++fs)
