add_library(
  linglong_api
  ./v1/types/CommonOptions.cpp
  ./v1/types/CommonOptions.h
  ./v1/types/CommonReply.cpp
  ./v1/types/CommonReply.h
  ./v1/types/DBusProxyOptions.cpp
  ./v1/types/DBusProxyOptions.h
  ./v1/types/ExecuteOptions.cpp
  ./v1/types/ExecuteOptions.h
  ./v1/types/StartOptions.cpp
  ./v1/types/StartOptions.h
)
target_link_libraries(linglong_api PUBLIC QSerializer::DBus)
