cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

project(
  ytj
  VERSION 0.1.0
  DESCRIPTION
    "A header only cpp library and a command line tool to convert yaml to json."
  HOMEPAGE_URL "https://github.com/black-desk/yt"
  LANGUAGES CXX)

option(YTJ_BUILD_EXAMPLES "Build examples of ytj or not."
       ${PROJECT_IS_TOP_LEVEL})

option(YTJ_ENABLE_TESTING "Enable test of ytj or not." ${PROJECT_IS_TOP_LEVEL})

option(YTJ_INSTALL "Enable install target or not." ${PROJECT_IS_TOP_LEVEL})

include(./cmake/GitSemver.cmake)

gitsemver(PROJECT_VERSION)

include(./cmake/CPM.cmake)

CPMFindPackage(
  NAME nlohmann_json
  VERSION 3.11.2
  URL "https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz"
  EXCLUDE_FROM_ALL ON
  OPTIONS "JSON_Install ON")

CPMFindPackage(
  NAME yaml-cpp
  VERSION 0.8.0
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG 0.8.0
  GIT_SHALLOW ON
  GIT_PROGRESS ON
  EXCLUDE_FROM_ALL ON
  OPTIONS "YAML_CPP_INSTALL ON")

if(NOT TARGET yaml-cpp::yaml-cpp)
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

include(./cmake/PFL.cmake)

pfl_init(ENABLE_TESTING ${YTJ_ENABLE_TESTING} BUILD_EXAMPLES
         ${YTJ_BUILD_EXAMPLES} INSTALL ${YTJ_INSTALL})

if(YTJ_ENABLE_TESTING)
  enable_testing()
endif()

# build library ytj
pfl_add_library(
  HEADER_ONLY
  VERSION
  ${PROJECT_VERSION}
  INS
  ./include/ytj/ytj.hpp.in
  SOURCES
  ./src/ytj/ytj.cpp
  APPS
  ytj
  COMPILE_FEATURES
  INTERFACE
  cxx_std_17
  LINK_LIBRARIES
  INTERFACE
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp)
