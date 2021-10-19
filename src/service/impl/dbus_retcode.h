/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    uap_install_file_not_exists = 401, // 文件不存在
    uap_install_directory_not_exists, // 目录不存在
    uap_install_failed, // uap 安转失败
    uap_name_format_error = 501, // uap包名格式错误
    uap_file_not_exists, // uap包不存在

    user_input_param_err = 600,
    pkg_already_installed,
    host_arch_not_recognized,
    update_appstream_failed,
    search_pkg_by_appstream_failed,
    load_ouap_file_failed,
    load_pkg_data_failed,
    extract_ouap_failed,
    ouap_install_file_not_exists, // 文件不存在
    ouap_install_failed,
    ouap_download_success,
    ouap_install_success, // ouap文件安装正确
    pkg_install_success, // 安装正确
};
template <typename T=quint32>
inline T RetCode(RetCode code){
    return static_cast<T>(code);
}
} // namespace dbus
} // namespace linglong
