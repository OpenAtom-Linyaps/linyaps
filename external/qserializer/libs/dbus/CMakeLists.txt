if(NOT QSERIALIZER_ENABLE_DBUS)
        return ()
endif()

if(NOT QT_DBUS_FOUND)
        message (
                FATAL_ERROR
                        "QSerializer::DBus need Qt::DBus."
        )
endif()

add_subdirectory (src)

if(NOT QSERIALIZER_ENABLE_INSTALL)
        return ()
endif()

include (GNUInstallDirs)

# cmake-format: off
set_target_properties (
        qserializer_dbus
        PROPERTIES
                OUTPUT_NAME qserializer_dbus
                EXPORT_NAME DBus
)
# cmake-format: on

install (
        TARGETS
                qserializer_dbus
        EXPORT
                qserializer_dbus
        FILE_SET public_headers
)

install (
        EXPORT
                qserializer_dbus
        DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
        FILE DBus.cmake
        NAMESPACE
                QSerializer::
)
