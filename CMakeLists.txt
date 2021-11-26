cmake_minimum_required(VERSION 3.0)

project(linglong)

# 保证项目兼容性
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(APP_VERSION "1.0")
set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
set(VERSION "1.0")

# configure debug mode and asan
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose Release or Debug" FORCE)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  if (NOT CMAKE_DEBUG_ASAN)
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0  -Wall -g -ggdb3")
    set(CMAKE_CXX_FLAGS "-O0 -Wall -g -ggdb3")
  else ()
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=undefined,address -Wall -g -ggdb3")
    set(CMAKE_CXX_FLAGS "-O0 -fsanitize=undefined,address -Wall -g -ggdb3")
  endif ()
endif ()

# thread --> tasn ; memory --> asan ; default --> none
if (NOT CMAKE_DEBUG_SANIT)
set(CMAKE_DEBUG_SANIT "none" CACHE STRING "Choose [none,thread,memory]" FORCE)
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("STATUS" "Compiler Feature: " ${CMAKE_DEBUG_SANIT})
if(CMAKE_DEBUG_SANIT STREQUAL "thread")
  set(CMAKE_CXX_FLAGS_RELEASE "")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=thread -Wall -g -ggdb3")
  set(CMAKE_CXX_FLAGS "-O0 -fsanitize=thread -Wall -g -ggdb3")
elseif(CMAKE_DEBUG_SANIT STREQUAL "memory")
  set(CMAKE_CXX_FLAGS_RELEASE "")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=undefined,address -Wall -g -ggdb3")
  set(CMAKE_CXX_FLAGS "-O0 -fsanitize=undefined,address -Wall -g -ggdb3")
elseif(CMAKE_DEBUG_SANIT STREQUAL "none")
  set(CMAKE_CXX_FLAGS_RELEASE "")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0  -Wall -g -ggdb3")
  set(CMAKE_CXX_FLAGS "-O0 -Wall -g -ggdb3")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0  -Wall -g -ggdb3")
  set(CMAKE_CXX_FLAGS "-O0 -Wall -g -ggdb3")
endif()
endif()

# debug mode
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    ADD_DEFINITIONS(-DDEBUG)
endif()


include(GNUInstallDirs)
include_directories(${linglong_SOURCE_DIR}/src)

add_subdirectory(${linglong_SOURCE_DIR}/third_party)
add_subdirectory(src)
add_subdirectory(test)

MESSAGE(STATUS "output " ${PROJECT_BINARY_DIR})
MESSAGE(STATUS "source " ${PROJECT_SOURCE_DIR})
MESSAGE(STATUS "install " ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})