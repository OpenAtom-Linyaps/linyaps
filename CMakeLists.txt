cmake_minimum_required(VERSION 3.0.0)

project(linglong)

# 保证项目兼容性
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)
include_directories(${linglong_SOURCE_DIR}/src)
#include_directories(${linglong_SOURCE_DIR}/third_party)

add_subdirectory(src)

add_subdirectory(test)
