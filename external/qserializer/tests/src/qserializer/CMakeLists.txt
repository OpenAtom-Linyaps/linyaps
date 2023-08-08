add_subdirectory (test_types)

# NOTE: It have to be an OBJECT library. Otherwise the linker will findout the
# test executable do not have reference to any symbols in this qserializer_TEST
# library and refuse to link against it. https://stackoverflow.com/a/53120509
add_library (
        qserializer_TEST OBJECT
        ${PROJECT_SOURCE_DIR}/tests/src/qserializer/basics.cpp
        ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_utils.cpp
)

target_sources (
        qserializer_TEST
        PUBLIC
                FILE_SET HEADERS BASE_DIRS ${PROJECT_SOURCE_DIR}/tests/src
                FILES ${PROJECT_SOURCE_DIR}/tests/src/qserializer/test_utils.h
)

target_link_libraries (
        qserializer_TEST
        PUBLIC
                GTest::gtest qserializer_testtypes_TEST
)
