cmake_minimum_required (
        VERSION
                3.24
)

project (
        qserializer
        LANGUAGES
                CXX
        VERSION
                0.1.1
)

include (${PROJECT_SOURCE_DIR}/misc/cmake/GetSemverFromGit.cmake)
set (QSERIALIZER_SEMVER ${PROJECT_VERSION})
qserializer_get_semver_from_git (QSERIALIZER_SEMVER)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)

macro(qserializer_find_dependency)
        find_package (${ARGN})
endmacro()

include (${PROJECT_SOURCE_DIR}/misc/cmake/FindQSerializerDependencies.cmake)

if(TARGET Qt::DBus)
        set (QT_DBUS_FOUND ON)
endif()

# cmake-format: off
set (
        QSERIALIZER_ENABLE_TESTING ${PROJECT_IS_TOP_LEVEL}
        CACHE BOOL "Build tests."
)

set (
        QSERIALIZER_ENABLE_EXAMPLES ${PROJECT_IS_TOP_LEVEL}
        CACHE BOOL "Build examples."
)

set (
        QSERIALIZER_ENABLE_INSTALL ${PROJECT_IS_TOP_LEVEL}
        CACHE BOOL "Enable the install target."
)

set (
        QSERIALIZER_ENABLE_DBUS ${QT_DBUS_FOUND}
        CACHE BOOL "Enable DBus support."
)
# cmake-format: on

add_subdirectory (libs)

if(QSERIALIZER_ENABLE_TESTING)
        # Build tests
        include (CTest)
        include (GoogleTest)
        find_package (GTest REQUIRED)
        add_subdirectory (tests)
endif()

if(QSERIALIZER_ENABLE_EXAMPLES)
        # Build examples
        add_subdirectory (examples)
endif()

# Install

if(NOT QSERIALIZER_ENABLE_INSTALL)
        return ()
endif()

include (CMakePackageConfigHelpers)
# This will be used to replace @PACKAGE_cmakeModulesDir@
set (cmakeModulesDir cmake)

# cmake-format: off
configure_package_config_file(
        misc/cmake/QSerializerConfig.cmake.in
        misc/cmake/QSerializerConfig.cmake
        INSTALL_DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
        PATH_VARS
                cmakeModulesDir
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
# cmake-format: on

# cmake-format: off
install (
        FILES
                ${PROJECT_SOURCE_DIR}/misc/cmake/FindQSerializerDependencies.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/QSerializerConfig.cmake
        DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
)
# cmake-format: on
