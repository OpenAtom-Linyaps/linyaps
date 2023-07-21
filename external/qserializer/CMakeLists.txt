cmake_minimum_required (
        VERSION
                3.13.4
)

project (
        qserializer
        LANGUAGES
                CXX
)

# cmake-format: off
set (
        QSERIALIZER_QT_VERSION_MAJOR "6"
        CACHE STRING "The qt major version tests and examples link to."
)

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
# cmake-format: on

message ("QSerializer build with QT${QSERIALIZER_QT_VERSION_MAJOR}")

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)

find_package (
        Qt${QSERIALIZER_QT_VERSION_MAJOR}
        COMPONENTS
                Core DBus
        REQUIRED
)

add_library (QSerializerCore INTERFACE)
add_library (QSerializer::Core ALIAS QSerializerCore)

# cmake-format: off
target_compile_features (QSerializerCore INTERFACE cxx_std_17)
target_sources (
        QSerializerCore
        PUBLIC
                FILE_SET HEADERS
                BASE_DIRS include
                FILES
                        include/QSerializer/QSerializer.h
)
# cmake-format: on

target_link_libraries (
        QSerializerCore
        INTERFACE
                Qt::Core
)

add_library (QSerializerDBus INTERFACE)
add_library (QSerializer::DBus ALIAS QSerializerDBus)

# cmake-format: off
target_compile_features (QSerializerDBus INTERFACE cxx_std_17)
target_sources (
        QSerializerDBus
        PUBLIC
                FILE_SET HEADERS
                BASE_DIRS include
                FILES
                        include/QSerializer/QSerializerDBus.h
)
# cmake-format: on

target_link_libraries (
        QSerializerDBus
        INTERFACE
                QSerializer::Core Qt::DBus
)

if(QSERIALIZER_ENABLE_TESTING)
        # Build tests
        include (CTest)
        include (GoogleTest)
        enable_testing ()
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

include (GNUInstallDirs)

# cmake-format: off
install (
        TARGETS QSerializerCore
        EXPORT QSerializerCore
        FILE_SET HEADERS
)

install (
        EXPORT QSerializerCore
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
        FILE QSerializerCore.cmake
        NAMESPACE QSerializer::
        COMPONENT Core
)

install (
        TARGETS QSerializerDBus
        EXPORT QSerializerDBus
        FILE_SET HEADERS
)

install (
        EXPORT QSerializerDBus
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
        FILE QSerializerDBus.cmake
        NAMESPACE QSerializer::
        COMPONENT DBus
)
# cmake-format: on

include (CMakePackageConfigHelpers)
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
                ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/QSerializerConfig.cmake
        DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/QSerializer
)
# cmake-format: on
