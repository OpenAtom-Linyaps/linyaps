if(NOT OCPPI_WITH_SPDLOG)
  return()
endif()

pfl_add_executable(
  OUTPUT_NAME
  with-logger
  DISABLE_INSTALL
  SOURCES
  src/main.cpp
  LINK_LIBRARIES
  PUBLIC
  linglong::ocppi
  spdlog::spdlog)
