if(NOT DEFINED QSERIALIZER_QT_VERSION_MAJOR)
        set (
                QSERIALIZER_QT_VERSION_MAJOR
                6
                CACHE
                        STRING "The Qt version QSerializer using."
        )
endif()

message (
        STATUS
                "QSerializer build with QT${QSERIALIZER_QT_VERSION_MAJOR}"
)

qserializer_find_dependency (
        Qt${QSERIALIZER_QT_VERSION_MAJOR}
        COMPONENTS
        Core
        REQUIRED
)

qserializer_find_dependency (Qt${QSERIALIZER_QT_VERSION_MAJOR} COMPONENTS DBus)
