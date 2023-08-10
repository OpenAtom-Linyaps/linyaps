add_library(
  linglong_api_v1_types
  CommonOptions.cpp
  CommonReply.cpp
  DBusProxyOptions.cpp
  ExecuteOptions.cpp
  StartOptions.cpp
)

target_sources(
  linglong_api_v1_types
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LINGLONG_FILESET_BASEDIRS}
         FILES
         CommonOptions.h
         CommonReply.h
         DBusProxyOptions.h
         ExecuteOptions.h
         StartOptions.h
)

target_link_libraries(linglong_api_v1_types PUBLIC QSerializer::DBus)
