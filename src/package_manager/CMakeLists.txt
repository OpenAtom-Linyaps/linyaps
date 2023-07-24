add_library(
  package-manager
  job.cpp
  job.h
  job_manager.cpp
  job_manager.h
  package_manager.cpp
  package_manager.h
  package_manager_p.h
)
target_link_libraries(package-manager PUBLIC linglong_lib)
