/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
// uap  error code
#define UAP_INSTALL_TRUE 400 //安装正确
#define FILE_NO_EXISTS_ERROR 401 //文件不存在
#define DIR_NO_EXISTS_ERROR 402 //目录不存在
#define UAP_NAME_ERROR 403 // uap包名格式错误
#define UAP_NO_EXISTS_ERROR 404 // uap包不存在
#define UAP_INSTALL_ERROR 405 // uap 安转失败

namespace linglong {
namespace dbus {
enum class RetCode {
    uap_install_success = 400, // 安装正确
    DataMakeSquashfsSuccess, //mksquashfs success
    uap_install_file_not_exists = 401, // 文件不存在
    uap_install_directory_not_exists, // 目录不存在
    uap_install_failed, // uap 安转失败
    uap_name_format_error = 501, // uap包名格式错误
    uap_file_not_exists, // uap包不存在
    DataDirNotExists,      //数据目录不存在
    UapJsonFileNotExists,  //uap.json不存在
    UapJsonFormatError,      //uap.json format error
    DataEntriesDirNotExists,     //entries 目录不存在
    DataFilesDirNotExists,     //files 目录不存在
    DataInfoJsonNotExists,     //info.json 文件不存在
    DataMakeSquashfsError,      //mksquashfs failed
    MakeTempWorkDirError,       //制作临时目录失败

    user_input_param_err = 600,
    pkg_already_installed,
    pkg_not_installed,
    host_arch_not_recognized,
    update_appstream_failed,
    install_runtime_failed,
    search_pkg_by_appstream_failed,
    load_ouap_file_failed,
    load_pkg_data_failed,
    extract_ouap_failed,
    ouap_install_file_not_exists, // 文件不存在
    ouap_install_failed,
    ouap_download_success,
    ouap_install_success, // ouap文件安装成功
    pkg_install_success, // 安装成功
    pkg_uninstall_success, // 卸载成功
    pkg_uninstall_failed   // 卸载失败
};
template <typename T=quint32>
inline T RetCode(RetCode code){
    return static_cast<T>(code);
}
} // namespace dbus
} // namespace linglong
