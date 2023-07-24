add_library(
  builder
  builder.h
  builder_config.cpp
  builder_config.h
  builder_templates.qrc
  depend_fetcher.cpp
  depend_fetcher.h
  linglong_builder.cpp
  linglong_builder.h
  project.cpp
  project.h
  source_fetcher.cpp
  source_fetcher.h
  source_fetcher_p.h
)

target_link_libraries(
  builder
  PUBLIC linglong_lib
         PkgConfig::glib2
         PkgConfig::ostree1
)
