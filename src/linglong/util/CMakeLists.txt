add_subdirectory(config)
add_subdirectory(http)
add_subdirectory(oci)
add_subdirectory(qserializer)
add_subdirectory(test)
add_subdirectory(version)

configure_file(configure.h.in configure.h)

add_library(
  linglong_util
  app_status.cpp
  appinfo_cache.cpp
  command_helper.cpp
  configure.cpp
  connection.cpp
  desktop_entry.cpp
  env.cpp
  erofs.cpp
  error.cpp
  file.cpp
  runner.cpp
  singleton.cpp
  status_code.cpp
  sysinfo.cpp
  uuid.cpp
  xdg.cpp
)

linglong_private_library_add_headers(
  linglong_util
  app_status.h
  appinfo_cache.h
  command_helper.h
  ${CMAKE_CURRENT_BINARY_DIR}/configure.h
  connection.h
  desktop_entry.h
  env.h
  erofs.h
  error.h
  file.h
  runner.h
  singleton.h
  status_code.h
  sysinfo.h
  uuid.h
  xdg.h
)

target_link_libraries(
  linglong_util
  PUBLIC linglong_util_config
         linglong_util_http
         linglong_util_oci
         linglong_util_qserializer
         linglong_util_test
         linglong_util_version
         linglong_FilesystemHelper_interface
         Qt::Core
         Qt::Sql
)
