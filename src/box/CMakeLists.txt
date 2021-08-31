add_executable(ll-box
               ../module/util/common.cpp
               util/logger.cpp
               util/filesystem.cpp
               container/container.cpp
               container/semaphore.cpp
               main.cpp)

target_link_libraries(ll-box stdc++)

install(TARGETS ll-box RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID)