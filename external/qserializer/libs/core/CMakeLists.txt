add_subdirectory (src)

if(NOT QSERIALIZER_ENABLE_INSTALL)
        return ()
endif()

include (GNUInstallDirs)

# cmake-format: off
set_target_properties (
        qserializer_core
        PROPERTIES
                OUTPUT_NAME qserializer_core
                EXPORT_NAME Core
)
# cmake-format: on

install (
        TARGETS
                qserializer_core
        EXPORT
                qserializer_core
        FILE_SET public_headers
)

install (
        EXPORT
                qserializer_core
        DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
        FILE Core.cmake
        NAMESPACE
                QSerializer::
)
