add_library (
        qserializer_examples_dbusmessage
        ${PROJECT_SOURCE_DIR}/src/qserializer/examples/dbus_message/Response.cpp
)

target_sources (
        qserializer_examples_dbusmessage
        PUBLIC
                FILE_SET public_headers TYPE HEADERS BASE_DIRS
                ${PROJECT_SOURCE_DIR}/include FILES
                ${PROJECT_SOURCE_DIR}/include/qserializer/examples/dbus_message/Response.h
)

target_link_libraries (
        qserializer_examples_dbusmessage
        PUBLIC
                QSerializer::DBus
)
