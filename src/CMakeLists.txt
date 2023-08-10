add_subdirectory(api)

add_subdirectory(dbusgen)

add_subdirectory(module)

add_subdirectory(package_manager)

add_subdirectory(service)

add_subdirectory(system_helper)

add_subdirectory(builder)

add_library(linglong_utils_serialize utils/serialize/yaml.cpp)
target_sources(
  linglong_utils_serialize
  PUBLIC FILE_SET
         private_headers
         TYPE
         HEADERS
         BASE_DIRS
         .
         FILES
         utils/serialize/yaml.h
         utils/serialize/json.h
)
target_link_libraries(linglong_utils_serialize PUBLIC QSerializer::Core)

add_library(linglong_utils INTERFACE)
target_link_libraries(linglong_utils INTERFACE linglong_utils_serialize)

add_executable(ll-service ./ll-service.main.cpp)
target_link_libraries(ll-service PRIVATE linglong::service)
install(TARGETS ll-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ll-system-helper ./ll-system-helper.main.cpp)
target_link_libraries(ll-system-helper PRIVATE linglong::system_helper)
install(TARGETS ll-system-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
