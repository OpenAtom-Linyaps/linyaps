set(TARGET_NAME qdbusxml2cpp)
set(BIN_NAME ${TARGET_NAME})

add_executable(${BIN_NAME}
  qdbusxml2cpp.cpp
)

target_link_libraries(
  ${BIN_NAME} PRIVATE
  Qt::DBus
  Qt::Core
  Qt::DBusPrivate
)
