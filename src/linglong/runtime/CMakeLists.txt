add_library(
  linglong_runtime
  app.cpp
  app_config.cpp
  container.cpp
  oci.cpp
  resource/app_configs.qrc
)

linglong_private_library_add_headers(
  linglong_runtime
  app.h
  app_config.h
  container.h
  oci.h
)

target_link_libraries(
  linglong_runtime
  PUBLIC Qt::Core
         linglong_util_qserializer
         linglong_package
)
