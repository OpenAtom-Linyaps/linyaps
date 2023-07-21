add_executable (tests)
target_sources (
        tests
        PRIVATE
                Base.h Base.cpp Book.h Book.cpp Page.h Page.cpp tests.cpp
)

target_link_libraries (
        tests
        PRIVATE
                QSerializer::Core GTest::gtest_main
)

gtest_discover_tests (tests)

if(QSERIALIZER_QT_VERSION_MAJOR STREQUAL "6")
        return ()
endif()

target_sources (
        tests
        PRIVATE
                custom_converter.h custom_converter.cpp
)
