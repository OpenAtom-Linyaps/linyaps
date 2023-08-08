add_library (
        qserializer_testtypes_TEST
        ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Base.cpp
        ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Book.cpp
        ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Page.cpp
)

target_sources (
        qserializer_testtypes_TEST
        PUBLIC
                FILE_SET HEADERS BASE_DIRS ${PROJECT_SOURCE_DIR}/tests/src
                FILES
                ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Base.h
                ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Book.h
                ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_types/Page.h
)

target_link_libraries (
        qserializer_testtypes_TEST
        PUBLIC
                QSerializer::Core
)
