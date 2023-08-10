add_subdirectory(api)
add_subdirectory(utils)
add_subdirectory(builder)

add_library(linglong INTERFACE)
target_link_libraries(
  linglong
  INTERFACE linglong_api
            linglong_utils
            linglong_builder
)
