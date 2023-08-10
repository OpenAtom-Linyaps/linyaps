add_library(linglong_utils_serialize yaml.cpp)

target_sources(
  linglong_utils_serialize
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LINGLONG_FILESET_BASEDIRS}
         FILES
         json.h
         yaml.h
)

target_link_libraries(linglong_utils_serialize PUBLIC QSerializer::Core)
