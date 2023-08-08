add_library (
        qserializer_dbus OBJECT
        ${PROJECT_SOURCE_DIR}/libs/dbus/src/qserializer/dbus.cpp
        ${PROJECT_SOURCE_DIR}/libs/dbus/src/qserializer/detail/dbus/operators.cpp
)

target_compile_features (
        qserializer_dbus PUBLIC
                                 cxx_std_17
)
target_sources (
        qserializer_dbus
        PUBLIC
                FILE_SET public_headers TYPE HEADERS BASE_DIRS
                ${PROJECT_SOURCE_DIR}/libs/dbus/include FILES
                ${PROJECT_SOURCE_DIR}/libs/dbus/include/qserializer/dbus.h
                ${PROJECT_SOURCE_DIR}/libs/dbus/include/qserializer/detail/dbus/operators.h
)

target_link_libraries (
        qserializer_dbus
        PUBLIC
                QSerializer::Core Qt::DBus
)

add_library (QSerializer::DBus ALIAS qserializer_dbus)
