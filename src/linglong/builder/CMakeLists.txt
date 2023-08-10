add_library(
  linglong_builder
  builder_config.cpp
  builder_templates.qrc
  depend_fetcher.cpp
  linglong_builder.cpp
  project.cpp
  source_fetcher.cpp
)

linglong_private_library_add_headers(
  linglong_builder
  builder_config.h
  builder.h
  project.h
  depend_fetcher.h
  linglong_builder.h
  source_fetcher.h
  source_fetcher_p.h
)

target_link_libraries(
  linglong_builder
  PUBLIC Qt::Core
         PkgConfig::glib2
         PkgConfig::ostree1
         linglong_util
)
