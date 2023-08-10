add_executable(ll-cli main.cpp)
target_link_libraries(
  ll-cli
  PRIVATE linglong
          linglong_AppManager_interface
          linglong_PackageManager_interface
)
