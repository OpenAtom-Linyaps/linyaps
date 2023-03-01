set(LL_SYSTEM_HELPER_SOURCES
    # cmake-format: sortable
    ../module/package/info.cpp
    ../module/package/info.h
    ../module/package/ref.h
    ../module/runtime/app_config.h
    ../module/runtime/oci.h
    ../module/util/file.cpp
    ../module/util/file.h
    ../module/util/log/log_handler.cpp
    ../module/util/serialize/json.h
    ../module/util/serialize/serialize.h
    ../module/util/sysinfo.cpp
    main.cpp
    privilege/privilege_install_portal.cpp
    privilege/privilege_install_portal.h
    system_helper.cpp
    system_helper.h
)

add_executable(ll-system-helper ${LL_SYSTEM_HELPER_SOURCES})

target_link_libraries(
  ll-system-helper
  PRIVATE yaml-cpp
          Qt5::Core
          Qt5::DBus
          LLDBusIPC
)

install(TARGETS ll-system-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
