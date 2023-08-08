add_subdirectory (qserializer)

add_executable (qserializer_tests)

target_link_libraries (
        qserializer_tests
        PUBLIC
                qserializer_TEST GTest::gtest_main
)

gtest_discover_tests (qserializer_tests)
