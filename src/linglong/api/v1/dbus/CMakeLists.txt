add_subdirectory(interfaces)

add_library(linglong_api_v1_dbus INTERFACE)

target_link_libraries(
  linglong_api_v1_dbus INTERFACE linglong_api_v1_dbus_interfaces
)
