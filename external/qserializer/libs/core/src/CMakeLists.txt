configure_file (
        ${PROJECT_SOURCE_DIR}/libs/core/include/qserializer/config.h.in
        ${PROJECT_BINARY_DIR}/libs/core/include/qserializer/config.h @ONLY
)

add_library (
        qserializer_core OBJECT
        ${PROJECT_SOURCE_DIR}/libs/core/src/qserializer/core.cpp
        ${PROJECT_SOURCE_DIR}/libs/core/src/qserializer/detail/QSerializer.cpp
)

target_compile_features (
        qserializer_core PUBLIC
                                 cxx_std_17
)
target_sources (
        qserializer_core
        PUBLIC
                FILE_SET public_headers TYPE HEADERS BASE_DIRS
                ${PROJECT_SOURCE_DIR}/libs/core/include
                ${PROJECT_BINARY_DIR}/libs/core/include FILES
                ${PROJECT_SOURCE_DIR}/libs/core/include/qserializer/core.h
                ${PROJECT_BINARY_DIR}/libs/core/include/qserializer/config.h
                ${PROJECT_SOURCE_DIR}/libs/core/include/qserializer/detail/QSerializer.h
)

target_link_libraries (
        qserializer_core
        PUBLIC
                Qt::Core
)

add_library (QSerializer::Core ALIAS qserializer_core)
