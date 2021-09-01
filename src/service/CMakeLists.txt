set(LL_SERVICE_SOURCES
    main.cpp
    impl/package_manager.cpp
    impl/job_manager.cpp
    impl/job_manager.h
    impl/job.cpp
    impl/job.h
    ../module/util/json.h
    ../module/util/uuid.cpp
    ../module/runtime/oci.h
    ../module/runtime/container.h
    ../module/uab/uab.cpp
    ../module/uab/uab.h
    ../module/uab/uap.cpp
    ../module/uab/uap.h
    ../module/runtime/app.cpp)

qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                     impl/package_manager.h
                     PackageManager)
qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.JobManager.xml
                     impl/job_manager.h
                     JobManager)
qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.Job.xml
                     impl/job.h
                     Job)

add_executable(ll-service
               ${LL_SERVICE_SOURCES})

target_link_libraries(ll-service PRIVATE ${LINK_LIBS})

install(TARGETS ll-service RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})