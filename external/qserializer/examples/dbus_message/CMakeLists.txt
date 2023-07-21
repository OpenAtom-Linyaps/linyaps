add_subdirectory (src/dbusgen)

add_executable (server)

target_sources (
        server
        PRIVATE
                src/Server.cpp src/Response.cpp src/server.main.cpp
)
target_include_directories (
        server
        PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/src
)
target_link_libraries (
        server
        PRIVATE
                QSerializer::DBus TestDBusServerApaptor
)

add_executable (client)
target_sources (
        client
        PRIVATE
                src/client.main.cpp src/Response.cpp
)
target_include_directories (
        client
        PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/src
)
target_link_libraries (
        client
        PRIVATE
                QSerializer::DBus TestDBusServerInterface
)
