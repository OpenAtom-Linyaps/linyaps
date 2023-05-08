add_executable(ll-prototype-virtual-base)
aux_source_directory(. LL_PVB_SOURCES)
target_sources(ll-prototype-virtual-base PRIVATE ${LL_PVB_SOURCES})
target_link_libraries(
  ll-prototype-virtual-base
  Qt::Core
)
