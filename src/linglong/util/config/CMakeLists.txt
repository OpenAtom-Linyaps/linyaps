add_library(linglong_util_config ./config.cpp)

linglong_private_library_add_headers(linglong_util_config config.h)

target_link_libraries(
  linglong_util_config
  PUBLIC Qt::Core
         linglong_util_qserializer
         linglong_repo
)
