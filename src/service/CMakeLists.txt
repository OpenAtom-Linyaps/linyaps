set(LL_SERVICE_SOURCES
    main.cpp
    resource/resource.qrc
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
    #${JSONSTRUCT_INCLUDE_DIR}/json_struct.h
    ../module/uab/uap.cpp
    ../module/uab/uap.h
    ../module/runtime/app.cpp
    ../module/util/fs.cpp
    ../module/util/fs.h
    ../module/runtime/tracker.h
    ../module/runtime/tracker.cpp

    # begin add by hqh debug 2021-9-16
    ../module/util/HttpClient.cpp
    ../module/util/HttpClient.h
    ../module/repo/RepoHelper.cpp
    ../module/repo/RepoHelper.h
    # end add by hqh debug 2021-9-16
    )

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

# begin add by hqh debug 2021-9-16
MESSAGE(STATUS "current CPU ARCH is: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")

#添加 libcurl库 需要安装 libcurl4-gnutls-dev
set(CURL_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libcurl.so)
#添加库glib和gio库 需要安装libglib2.0-dev
set(GLIB_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libglib-2.0.so /usr/lib/x86_64-linux-gnu/libgio-2.0.so)
set(GOBJ_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libgobject-2.0.so)
#添加ostree 需要安装libostree-dev
set(OSTREE_LIBRARYDIR /usr/lib/x86_64-linux-gnu/libostree-1.so)

#libcurl头文件
set(CURL_INCLUDEDIRS /usr/include/x86_64-linux-gnu/)
#添加头文件目录
set(GLIB_INCLUDEDIRS /usr/include/glib-2.0/ /usr/lib/x86_64-linux-gnu/glib-2.0/include/)
set(OSTREE_INCLUDEDIRS /usr/include/ostree-1/)
set(GOBJ_INCLUDEDIRS /usr/include/glib-2.0/)
endif()
# end add by hqh debug 2021-9-16

add_executable(ll-service
               ${LL_SERVICE_SOURCES})

# begin modify by hqh debug 2021-9-16
target_include_directories(ll-service
PRIVATE
#${JSONSTRUCT_INCLUDE_DIR}
${CURL_INCLUDEDIRS}
${GLIB_INCLUDEDIRS}
${OSTREE_INCLUDEDIRS}
${GOBJ_INCLUDEDIRS}
)

target_link_libraries(ll-service
PRIVATE
${LINK_LIBS}
${CURL_LIBRARYDIR}
${GLIB_LIBRARYDIR}
${OSTREE_LIBRARYDIR}
${GOBJ_LIBRARYDIR}
)
# end modify by hqh debug 2021-9-16

install(TARGETS ll-service RUNTIME DESTINATION bin)
