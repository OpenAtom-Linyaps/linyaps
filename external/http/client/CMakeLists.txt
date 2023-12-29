cmake_minimum_required(VERSION 3.2)

project(QtLinglongRepoClientAPI)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Gui)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(${PROJECT_NAME}
  Api_JSONResult.h
  Api_UploadTaskFileResp.h
  Api_UploadTaskLayerFileResp.h
  FuzzySearchApp_200_response.h
  GetRepo_200_response.h
  NewUploadTaskID_200_response.h
  Request_Auth.h
  Request_FuzzySearchReq.h
  Request_RegisterStruct.h
  Response_NewUploadTaskResp.h
  Response_SignIn.h
  Response_UploadTaskResp.h
  Response_UploadTaskStatusInfo.h
  Schema_NewUploadTaskReq.h
  Schema_RepoInfo.h
  SignIn_200_response.h
  UploadTaskInfo_200_response.h
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
  Api_UploadTaskFileResp.cpp
  Api_UploadTaskLayerFileResp.cpp
  FuzzySearchApp_200_response.cpp
  GetRepo_200_response.cpp
  NewUploadTaskID_200_response.cpp
  Request_Auth.cpp
  Request_FuzzySearchReq.cpp
  Request_RegisterStruct.cpp
  Response_NewUploadTaskResp.cpp
  Response_SignIn.cpp
  Response_UploadTaskResp.cpp
  Response_UploadTaskStatusInfo.cpp
  Schema_NewUploadTaskReq.cpp
  Schema_RepoInfo.cpp
  SignIn_200_response.cpp
  UploadTaskInfo_200_response.cpp
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
