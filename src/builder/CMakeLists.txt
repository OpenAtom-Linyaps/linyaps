add_library(
  linglong_lib_builder
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
add_library(
  linglong::builder
  ALIAS
  linglong_lib_builder
)

target_link_libraries(
  linglong_lib_builder
  PUBLIC linglong_lib
         PkgConfig::glib2
         PkgConfig::ostree1
)
