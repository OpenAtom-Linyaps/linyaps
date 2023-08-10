add_library(
  linglong_lib_service
  ./app_manager.cpp
  ./app_manager.h
  ./app_manager_p.h
)
add_library(
  linglong::service
  ALIAS
  linglong_lib_service
)
target_sources(linglong_lib_service PRIVATE ${LIB_LL_SERVICE_SOURCES})
target_link_libraries(
  linglong_lib_service
  PUBLIC linglong_lib
         linglong_packagemanager
         linglong_AppManager_adaptor
)
