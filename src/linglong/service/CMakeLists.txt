add_library(linglong_service app_manager.cpp)

linglong_private_library_add_headers(
  linglong_service
  app_manager.h
  app_manager_p.h
)

target_link_libraries(
  linglong_service
  PUBLIC linglong_dbusipc
         linglong_packagemanager
         linglong_AppManager_adaptor
)
