add_library(
  linglong_api_v1_types
  CommonOptions.cpp
  CommonReply.cpp
  DBusProxyOptions.cpp
  ExecuteOptions.cpp
  StartOptions.cpp
)

linglong_private_library_add_headers(
  linglong_api_v1_types
  CommonOptions.h
  CommonReply.h
  DBusProxyOptions.h
  ExecuteOptions.h
  StartOptions.h
)

target_link_libraries(linglong_api_v1_types PUBLIC QSerializer::DBus)
