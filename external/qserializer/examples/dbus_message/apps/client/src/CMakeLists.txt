set_source_files_properties (
        ${PROJECT_SOURCE_DIR}/api/dbus/io.github.black_desk.TestDBusServer.xml
        PROPERTIES
                INCLUDE qserializer/examples/dbus_message/Response.h CLASSNAME
                qserializer_examples_dbus_message_TestDBusServerInterface
                NO_NAMESPACE true
)

set (DBUS_SOURCES)
qt_add_dbus_interface (
        DBUS_SOURCES
        ${PROJECT_SOURCE_DIR}/api/dbus/io.github.black_desk.TestDBusServer.xml
        qserializer/examples/dbus_message/gen_TestDBusServerInterface
)

add_executable (
        qserializer_examples_dbusmessage_client
        ${DBUS_SOURCES} ${PROJECT_SOURCE_DIR}/apps/client/src/main.cpp
)

target_sources (
        qserializer_examples_dbusmessage_client
        PUBLIC
                FILE_SET private_header TYPE HEADERS BASE_DIRS
                ${PROJECT_SOURCE_DIR}/apps/client/src
                ${PROJECT_BINARY_DIR}/apps/client/src FILES
                ${PROJECT_SOURCE_DIR}/apps/client/src/qserializer/examples/dbus_message/TestDBusServerInterface.h
                ${PROJECT_BINARY_DIR}/apps/client/src/qserializer/examples/dbus_message/gen_TestDBusServerInterface.h
)

target_link_libraries (
        qserializer_examples_dbusmessage_client
        PRIVATE
                qserializer_examples_dbusmessage
)
