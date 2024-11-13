# Copyright 2005-2011 Kitware, Inc.
# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

include(MacroAddFileDependencies)

function(qt6_add_dbus_interface _sources _interface _relativename)
    get_filename_component(_infile ${_interface} ABSOLUTE)
    get_filename_component(_basepath ${_relativename} DIRECTORY)
    get_filename_component(_basename ${_relativename} NAME)
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_relativename}.h")
    set(_impl   "${CMAKE_CURRENT_BINARY_DIR}/${_relativename}.cpp")
    if(_basepath)
        set(_moc "${CMAKE_CURRENT_BINARY_DIR}/${_basepath}/moc_${_basename}.cpp")
    else()
        set(_moc "${CMAKE_CURRENT_BINARY_DIR}/moc_${_basename}.cpp")
    endif()

    get_source_file_property(_nonamespace ${_interface} NO_NAMESPACE)
    if(_nonamespace)
        set(_params -N -m)
    else()
        set(_params -m)
    endif()

    get_source_file_property(_classname ${_interface} CLASSNAME)
    if(_classname)
        set(_params ${_params} -c ${_classname})
    endif()

    get_source_file_property(_include ${_interface} INCLUDE)
    if(_include)
        set(_params ${_params} -i ${_include})
    endif()

    add_custom_command(OUTPUT "${_impl}" "${_header}"
        COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::qdbusxml2cpp ${_params} -p ${_relativename} ${_infile}
        DEPENDS ${_infile} ${QT_CMAKE_EXPORT_NAMESPACE}::qdbuscpp2xml
        VERBATIM
    )

    set_source_files_properties("${_impl}" "${_header}" PROPERTIES
        SKIP_AUTOMOC TRUE
        SKIP_AUTOUIC TRUE
    )

    qt6_generate_moc("${_header}" "${_moc}")

    list(APPEND ${_sources} "${_impl}" "${_header}")
    macro_add_file_dependencies("${_impl}" "${_moc}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()

function(qt6_add_dbus_adaptor _sources _xml_file _include) # _optionalParentClass _optionalRelativename _optionalClassName)
    get_filename_component(_infile ${_xml_file} ABSOLUTE)

    set(_optionalParentClass "${ARGV3}")
    if(_optionalParentClass)
        set(_parentClassOption "-l")
        set(_parentClass "${_optionalParentClass}")
    endif()

    set(_optionalRelativename "${ARGV4}")
    if(_optionalRelativename)
        set(_relativename ${_optionalRelativename})
    else()
        string(REGEX REPLACE "(.*[/\\.])?([^\\.]+)\\.xml" "\\2adaptor" _relativename ${_infile})
        string(TOLOWER ${_relativename} _relativename)
    endif()
    get_filename_component(_basepath ${_relativename} DIRECTORY)
    get_filename_component(_basename ${_relativename} NAME)

    set(_optionalClassName "${ARGV5}")
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_relativename}.h")
    set(_impl   "${CMAKE_CURRENT_BINARY_DIR}/${_relativename}.cpp")
    if(_basepath)
        set(_moc "${CMAKE_CURRENT_BINARY_DIR}/${_basepath}/moc_${_basename}.cpp")
    else()
        set(_moc "${CMAKE_CURRENT_BINARY_DIR}/moc_${_basename}.cpp")
    endif()

    if(_optionalClassName)
        add_custom_command(OUTPUT "${_impl}" "${_header}"
          COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::qdbusxml2cpp -m -a ${_relativename} -c ${_optionalClassName} -i ${_include} ${_parentClassOption} ${_parentClass} ${_infile}
          DEPENDS ${_infile} ${QT_CMAKE_EXPORT_NAMESPACE}::qdbuscpp2xml
          VERBATIM
        )
    else()
        add_custom_command(OUTPUT "${_impl}" "${_header}"
          COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::qdbusxml2cpp -m -a ${_relativename} -i ${_include} ${_parentClassOption} ${_parentClass} ${_infile}
          DEPENDS ${_infile} ${QT_CMAKE_EXPORT_NAMESPACE}::qdbuscpp2xml
          VERBATIM
        )
    endif()

    qt6_generate_moc("${_header}" "${_moc}")
    set_source_files_properties("${_impl}" "${_header}" PROPERTIES
        SKIP_AUTOMOC TRUE
        SKIP_AUTOUIC TRUE
    )
    macro_add_file_dependencies("${_impl}" "${_moc}")

    list(APPEND ${_sources} "${_impl}" "${_header}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()
