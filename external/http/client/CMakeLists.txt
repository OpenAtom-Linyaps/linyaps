cmake_minimum_required(VERSION 3.2)

project(QtLinglongRepoClientAPI)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Gui)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(${PROJECT_NAME}
  Api_JSONResult.h
  FuzzySearchApp_200_response.h
  Request_FuzzySearchReq.h
  Request_RegisterStruct.h
  ClientApi.h
  Helpers.h
  HttpRequest.h
  Object.h
  Enum.h
  HttpFileElement.h
  ServerConfiguration.h
  ServerVariable.h
  Oauth.h
  Api_JSONResult.cpp
  FuzzySearchApp_200_response.cpp
  Request_FuzzySearchReq.cpp
  Request_RegisterStruct.cpp
  ClientApi.cpp
  Helpers.cpp
  HttpRequest.cpp
  HttpFileElement.cpp
  Oauth.cpp
  )

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Gui

)
