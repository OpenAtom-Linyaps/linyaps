qt_add_dbus_adaptor (
        TEST_DBUS_SERVER_ADAPTOR_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../api/dbus/io.github.black_desk.TestDBusServer.xml
        Server.h
        Server
        TestDBusServerApaptor
)

add_library (TestDBusServerApaptor OBJECT ${TEST_DBUS_SERVER_ADAPTOR_SOURCES})
target_include_directories (
        TestDBusServerApaptor
        PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE
                ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries (
        TestDBusServerApaptor
        PUBLIC
                QSerializerDBus
)

set_source_files_properties (
        ${CMAKE_CURRENT_SOURCE_DIR}/../../api/dbus/io.github.black_desk.TestDBusServer.xml
        PROPERTIES
                INCLUDE Response.h
)

qt_add_dbus_interface (
        TEST_DBUS_SERVER_INTERFACE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../api/dbus/io.github.black_desk.TestDBusServer.xml
        TestDBusServerInterface
)

add_library (
        TestDBusServerInterface OBJECT ${TEST_DBUS_SERVER_INTERFACE_SOURCES}
)

target_include_directories (
        TestDBusServerInterface
        PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE
                ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries (
        TestDBusServerInterface
        PUBLIC
                QSerializerDBus
)
