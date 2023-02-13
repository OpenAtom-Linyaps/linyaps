cmake_minimum_required(VERSION 3.0)

project(linglong)

# 保证项目兼容性
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
set(VERSION "1.0")

# defalut set build type for release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING
            "Choose Release or Debug"
            FORCE
  )
endif()

# set sanitizer for debug that default disabled. thread --> tasn ; address -->
# asan ; default --> none
if(NOT CMAKE_DEBUG_SANIT)
  set(CMAKE_DEBUG_SANIT
      "none"
      CACHE STRING
            "Choose [none,thread,address]"
            FORCE
  )
endif()

if(CMAKE_BUILD_TYPE
   STREQUAL
   "Debug"
)
  add_definitions(-DDEBUG) # debug mode
  message(
    "STATUS"
    "Compiler Feature: "
    ${CMAKE_DEBUG_SANIT}
  )
  if(CMAKE_DEBUG_SANIT
     STREQUAL
     "thread"
  )
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=thread -Wall -g -ggdb3")
  elseif(
    CMAKE_DEBUG_SANIT
    STREQUAL
    "address"
  )
    set(CMAKE_CXX_FLAGS_DEBUG
        "-O0 -fsanitize=undefined,address -Wall -g -ggdb3"
    )
  else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
  endif()
endif()

include(GNUInstallDirs)
include_directories(${linglong_SOURCE_DIR}/src)

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(misc)

message(STATUS "output " ${PROJECT_BINARY_DIR})
message(STATUS "source " ${PROJECT_SOURCE_DIR})
message(STATUS "install " ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
