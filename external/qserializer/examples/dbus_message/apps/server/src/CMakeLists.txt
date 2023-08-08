set (DBUS_SOURCES)

qt_add_dbus_adaptor (
        DBUS_SOURCES
        ${PROJECT_SOURCE_DIR}/api/dbus/io.github.black_desk.TestDBusServer.xml
        ${PROJECT_SOURCE_DIR}/apps/server/src/qserializer/examples/dbus_message/Server.h
        qserializer::examples::dbus_message::Server
        qserializer/examples/dbus_message/gen_TestDBusServerAdaptor
        qserializer_examples_dbus_message_TestDBusServerAdaptor
)

add_executable (
        qserializer_examples_dbusmessage_server
        ${DBUS_SOURCES}
        ${PROJECT_SOURCE_DIR}/apps/server/src/main.cpp
        ${PROJECT_SOURCE_DIR}/apps/server/src/qserializer/examples/dbus_message/Server.cpp
)

target_sources (
        qserializer_examples_dbusmessage_server
        PUBLIC
                FILE_SET private_header TYPE HEADERS BASE_DIRS
                ${PROJECT_SOURCE_DIR}/apps/server/src
                ${PROJECT_BINARY_DIR}/apps/server/src FILES
                ${PROJECT_SOURCE_DIR}/apps/server/src/qserializer/examples/dbus_message/Server.h
                ${PROJECT_SOURCE_DIR}/apps/server/src/qserializer/examples/dbus_message/TestDBusServerAdaptor.h
                ${PROJECT_BINARY_DIR}/apps/server/src/qserializer/examples/dbus_message/gen_TestDBusServerAdaptor.h
)

target_link_libraries (
        qserializer_examples_dbusmessage_server
        PRIVATE
                qserializer_examples_dbusmessage
)
