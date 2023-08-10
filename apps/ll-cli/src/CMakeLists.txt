add_executable(ll-cli main.cpp)
target_link_libraries(
  ll-cli
  PRIVATE linglong_lib
          linglong::package_manager
          linglong::service
)
