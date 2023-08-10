add_subdirectory(v1)

add_library(linglong_api INTERFACE)
target_link_libraries(linglong_api INTERFACE linglong_api_v1)
