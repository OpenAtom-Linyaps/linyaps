add_executable(ll-system-helper ./ll-system-helper.main.cpp)
target_link_libraries(ll-system-helper PRIVATE linglong::system_helper)
