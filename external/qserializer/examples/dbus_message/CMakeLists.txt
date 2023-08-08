if(NOT QSERIALIZER_ENABLE_DBUS)
        return ()
endif()

project (
        qserializer_examples_dbusmessage
        LANGUAGES
                CXX
)

add_subdirectory (src)
add_subdirectory (apps)
