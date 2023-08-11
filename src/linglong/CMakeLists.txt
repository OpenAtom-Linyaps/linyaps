add_subdirectory(api)
add_subdirectory(utils)

add_subdirectory(dbus_ipc)
add_subdirectory(package)
add_subdirectory(repo)
add_subdirectory(runtime)
add_subdirectory(util)

add_subdirectory(builder)
add_subdirectory(package_manager)
add_subdirectory(service)
add_subdirectory(system_helper)

add_library(linglong INTERFACE)
target_link_libraries(
  linglong
  INTERFACE linglong_api
            linglong_utils
            linglong_builder
            linglong_packagemanager
            linglong_service
            linglong_dbusipc
            linglong_package
            linglong_repo
            linglong_runtime
            linglong_util
            linglong_systemhelper
)
