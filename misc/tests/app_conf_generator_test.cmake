# SPDX-FileCopyrightText: 2026 3219378872
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.11.4)

if(NOT DEFINED APP_CONF_GENERATOR)
  get_filename_component(
    APP_CONF_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../libexec/linglong/app-conf-generator"
    ABSOLUTE)
endif()

string(RANDOM LENGTH 12 nonce)
set(test_root "${CMAKE_CURRENT_BINARY_DIR}/app-conf-test-${nonce}")
file(MAKE_DIRECTORY "${test_root}")
set(app_id "org.example.Editor")
set(command "\"/opt/apps/${app_id}/files/bin/editor\" %U")
set(launch "/usr/bin/ll-cli run ${app_id} -- ${command}")

set(kinds desktop dbus systemd_lib systemd_share menu)
set(desktop_dir "share/applications")
set(desktop_suffix desktop)
set(desktop_input "[Desktop Entry]\nName=Editor\nExec=${command}\nTryExec=editor\n")
set(desktop_expected "[Desktop Entry]\nX-linglong=${app_id}\nName=Editor\nExec=${launch}\nTryExec=ll-cli\n")
set(dbus_dir "share/dbus-1/services")
set(dbus_suffix service)
set(dbus_input "[D-BUS Service]\nName=${app_id}\nExec=${command}\n")
set(dbus_expected "[D-BUS Service]\nName=${app_id}\nExec=${launch}\n")
set(systemd_lib_dir "lib/systemd/user")
set(systemd_share_dir "share/systemd/user")
foreach(kind systemd_lib systemd_share)
  set(${kind}_suffix service)
  set(${kind}_input "[Service]\nExecStart=${command}\n")
  set(${kind}_expected "[Service]\nExecStart=${launch}\n")
endforeach()
set(menu_dir "share/applications/context-menus")
set(menu_suffix conf)
set(menu_input "[Desktop Entry]\nName=Menu\nExec=${command}\n")
set(menu_expected "[Desktop Entry]\nName=Menu\nExec=${launch}\n")

function(run_generator label files)
  message(STATUS "Checking ${label}")
  # Match Builder::generateAppConf: bash -e helper app-id build-output.
  execute_process(
    COMMAND bash -e "${APP_CONF_GENERATOR}" "${app_id}" "${files}"
    WORKING_DIRECTORY "${test_root}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 10)
  if(NOT "${result}" STREQUAL "0")
    message(SEND_ERROR "${label}: generator failed (${result}): ${error}")
  elseif(NOT "${error}" STREQUAL "")
    message(SEND_ERROR "${label}: unexpected stderr: ${error}")
  endif()
endfunction()

function(check_contents label filename expected)
  file(READ "${filename}" actual)
  if(NOT "${actual}" STREQUAL "${expected}")
    message(SEND_ERROR "${label}: incorrect contents in ${filename}\nActual:\n${actual}")
  endif()
endfunction()

function(check_rewriting label project_name file_name)
  set(files "${test_root}/${project_name}/files")
  foreach(kind ${kinds})
    file(WRITE "${files}/${${kind}_dir}/${file_name}.${${kind}_suffix}"
         "${${kind}_input}")
  endforeach()
  run_generator("${label}" "${files}")
  foreach(kind ${kinds})
    check_contents("${label}/${kind}"
                   "${files}/${${kind}_dir}/${file_name}.${${kind}_suffix}"
                   "${${kind}_expected}")
  endforeach()
endfunction()

check_rewriting(plain plain editor)
check_rewriting(spaced_directory "project space" editor)
check_rewriting(spaced_filename filename "Editor Notes")
check_rewriting(multiline_filename multiline "Editor\nNotes")
check_rewriting(literal_glob_directory "project[v1]*" editor)

# Empty match sets and directories with a matching suffix are not input files.
set(files "${test_root}/empty/files")
foreach(kind ${kinds})
  file(WRITE "${files}/${${kind}_dir}/unrelated.txt" "Exec=unchanged\n")
  file(WRITE "${files}/${${kind}_dir}/ignored.${${kind}_suffix}/nested.txt"
       "Exec=nested\n")
endforeach()
run_generator(empty_matches "${files}")
foreach(kind ${kinds})
  check_contents(unrelated "${files}/${${kind}_dir}/unrelated.txt" "Exec=unchanged\n")
  check_contents(nested "${files}/${${kind}_dir}/ignored.${${kind}_suffix}/nested.txt"
                 "Exec=nested\n")
endforeach()

run_generator(no_config_directories "${test_root}/no-config-files")
file(REMOVE_RECURSE "${test_root}")
