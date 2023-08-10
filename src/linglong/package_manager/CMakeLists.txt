add_library(
  linglong_packagemanager
  job.cpp
  job_manager.cpp
  package_manager.cpp
)

target_sources(
  linglong_packagemanager
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LINGLONG_FILESET_BASEDIRS}
         FILES
         job.h
         job_manager.h
         package_manager.h
         package_manager_p.h
)

target_link_libraries(
  linglong_packagemanager
  PUBLIC linglong_lib
         linglong_PackageManager_adaptor
         linglong_JobManager_adaptor
         linglong_Job_adaptor
)
