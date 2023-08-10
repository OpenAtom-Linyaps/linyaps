add_library(linglong_util_oci ./distribution_client.cpp)
linglong_private_library_add_headers(linglong_util_oci ./distribution_client.h)

target_link_libraries(
  linglong_util_oci
  PUBLIC linglong_util_qserializer
         linglong_util_http
)
