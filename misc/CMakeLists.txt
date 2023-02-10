pkg_get_variable(
  SYSUSERSDIR
  systemd
  sysusersdir
)

install(FILES sysuser/linglong.conf DESTINATION ${SYSUSERSDIR})
