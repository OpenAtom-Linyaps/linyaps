add_library(
  linglong_lib_package_manager
  job.cpp
  job.h
  job_manager.cpp
  job_manager.h
  package_manager.cpp
  package_manager.h
  package_manager_p.h
)
add_library(
  linglong::package_manager
  ALIAS
  linglong_lib_package_manager
)
target_link_libraries(linglong_lib_package_manager PUBLIC linglong_lib)
