if(NOT OCPPI_WITH_SPDLOG)
  return()
endif()

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  src/main.cpp
  LINK_LIBRARIES
  PUBLIC
  ocppi::ocppi
  spdlog::spdlog)
