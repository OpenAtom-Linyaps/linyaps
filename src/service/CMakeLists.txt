add_library(
  service
  ./app_manager.cpp
  ./app_manager.h
  ./app_manager_p.h
)
target_sources(service PRIVATE ${LIB_LL_SERVICE_SOURCES})
target_link_libraries(service PUBLIC linglong_lib package-manager)
