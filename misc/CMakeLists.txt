# builder

set(BUILDER_TEMPLATE_FILES
    # cmake-format: sortable
    builder-template/autotools.yaml
    builder-template/cmake.yaml
    builder-template/default.yaml
    builder-template/example.yaml
    builder-template/qmake.yaml
)

set(BUILDER_CONFIG_FILE builder.yaml)

install(FILES ${BUILDER_TEMPLATE_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin/ll-builder/template
)
install(FILES ${BUILDER_CONFIG_FILE}
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linglong
)

# sysuser

pkg_get_variable(
  SYSUSERSDIR
  systemd
  sysusersdir
)

install(FILES sysuser/linglong.conf DESTINATION ${SYSUSERSDIR})
