add_library(linglong_service app_manager.cpp)

target_sources(
  linglong_service
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LINGLONG_FILESET_BASEDIRS}
         FILES
         app_manager.h
         app_manager_p.h
)

target_link_libraries(
  linglong_service
  PUBLIC linglong_lib
         linglong_packagemanager
         linglong_AppManager_adaptor
)
