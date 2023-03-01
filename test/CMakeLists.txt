find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(ll-test)

file(
  GLOB_RECURSE
  LL_TEST_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
)

aux_source_directory(. LL_TEST_SOURCES)
target_sources(ll-test PRIVATE ${LL_TEST_SOURCES})
target_link_libraries(ll-test PRIVATE GTest::gtest_main linglong_lib)

gtest_discover_tests(ll-test)
