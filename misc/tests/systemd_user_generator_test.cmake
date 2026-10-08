# SPDX-FileCopyrightText: 2026 Hanabi9249
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.11.4)

if(NOT DEFINED GENERATOR_TEMPLATE)
  set(GENERATOR_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../lib/systemd/user-generators/linglong-user-systemd-generator")
endif()

string(RANDOM LENGTH 12 nonce)
set(test_root "${CMAKE_CURRENT_BINARY_DIR}/systemd-generator-test-${nonce}")
file(MAKE_DIRECTORY "${test_root}")

function(check_generator label root_name populated)
  set(LINGLONG_ROOT "${test_root}/${label}/${root_name}")
  set(units "${LINGLONG_ROOT}/entries/lib/systemd/user")
  set(late "${test_root}/${label}/output late")
  set(layer "${test_root}/${label}/layer")
  file(MAKE_DIRECTORY "${late}" "${layer}")

  if(populated)
    file(MAKE_DIRECTORY "${units}")
    # Exported entries are symlinks to the installed application's unit files.
    foreach(kind service timer)
      set(unit "org.example.Editor.${kind}")
      file(WRITE "${layer}/${unit}" "[Unit]\nDescription=Fixture ${kind}\n")
      execute_process(COMMAND ln -s "${layer}/${unit}" "${units}/${unit}"
                      RESULT_VARIABLE result)
      if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${label}: cannot create fixture symlink")
      endif()
    endforeach()
  elseif(NOT "${label}" STREQUAL "missing")
    file(MAKE_DIRECTORY "${units}")
  endif()

  if("${label}" STREQUAL "literal_glob")
    # An unquoted root would expand to this different installation.
    set(decoy "${test_root}/${label}/root1-decoy/entries/lib/systemd/user")
    file(MAKE_DIRECTORY "${decoy}")
    file(WRITE "${decoy}/org.example.Decoy.service" "[Unit]\nDescription=Decoy\n")
  endif()

  set(generator "${test_root}/${label}/generator")
  configure_file("${GENERATOR_TEMPLATE}" "${generator}" @ONLY)
  execute_process(COMMAND sh "${generator}" "${test_root}/${label}/normal"
                          "${test_root}/${label}/early" "${late}"
                  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
                  TIMEOUT 10)
  if(NOT "${result}" STREQUAL "0")
    message(SEND_ERROR "${label}: generator failed (${result}): ${error}")
  endif()

  file(GLOB actual "${late}/*")
  list(LENGTH actual count)
  if(populated)
    if(NOT count EQUAL 2)
      message(SEND_ERROR "${label}: expected two exported units, got ${count}: ${error}")
    endif()
    foreach(kind service timer)
      set(unit "org.example.Editor.${kind}")
      if(NOT IS_SYMLINK "${late}/${unit}")
        message(SEND_ERROR "${label}: ${unit} is not an exported symlink")
      elseif(NOT EXISTS "${late}/${unit}")
        message(SEND_ERROR "${label}: ${unit} is a broken symlink")
      else()
        file(READ "${late}/${unit}" contents)
        if(NOT "${contents}" STREQUAL "[Unit]\nDescription=Fixture ${kind}\n")
          message(SEND_ERROR "${label}: wrong source contents for ${unit}")
        endif()
      endif()
    endforeach()
    if(NOT "${error}" STREQUAL "")
      message(SEND_ERROR "${label}: unexpected stderr: ${error}")
    endif()
  elseif(NOT count EQUAL 0)
    message(SEND_ERROR "${label}: empty or absent installation exported units")
  endif()
  message(STATUS "Checked ${label}: exit=${result}, exported=${count}")
endfunction()

check_generator(plain root TRUE)
check_generator(spaced "root space" TRUE)
check_generator(literal_glob "root[1]*" TRUE)
check_generator(empty root FALSE)
check_generator(missing root FALSE)

if(KEEP_FIXTURES)
  message(STATUS "Retained fixtures: ${test_root}")
else()
  # test_root is this invocation's exclusive random directory under the build cwd.
  file(REMOVE_RECURSE "${test_root}")
endif()
