add_library(
  linglong_builder
  builder_config.cpp
  builder_templates.qrc
  depend_fetcher.cpp
  linglong_builder.cpp
  project.cpp
  source_fetcher.cpp
)

target_sources(
  linglong_builder
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LINGLONG_FILESET_BASEDIRS}
         FILES
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
  PUBLIC linglong_lib
         PkgConfig::glib2
         PkgConfig::ostree1
)
