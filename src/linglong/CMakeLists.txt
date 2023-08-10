add_subdirectory(api)
add_subdirectory(utils)

add_subdirectory(builder)
add_subdirectory(package_manager)
add_subdirectory(service)

add_library(linglong INTERFACE)
target_link_libraries(
  linglong
  INTERFACE linglong_api
            linglong_utils
            linglong_builder
            linglong_packagemanager
            linglong_service
)
