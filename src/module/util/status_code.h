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

namespace linglong {
namespace util {
enum class StatusCode {
    kSuccess = 0, ///< 成功
    kFail = 1, ///< 失败
    /// ll-builder模块状态码范围为100~299
    kDataDirNotExists = 100, ///< 数据目录不存在
    kUapJsonFileNotExists, ///< uap.json不存在
    kBundleFileNotExists, ///< uab文件不存在
    /// ll-service模块状态码范围为600~899
    kUserInputParamErr = 600, ///< 用户输入参数错误
    kPkgAlreadyInstalled, ///< 已安装
    kPkgNotInstalled, ///< 未安装
    kInstallRuntimeFailed, ///< 安装runtime失败
    kLoadPkgDataFailed, ///< 加载应用数据失败
    kPkgInstallSuccess, ///< 安装成功
    kPkgInstalling, ///< 安装中
    kPkgInstallFailed, ///< 安装失败
    kPkgUninstallSuccess, ///< 卸载成功
    kPkgUninstalling, ///< 卸载中
    kPkgUninstallFailed, ///< 卸载失败
    kErrorPkgUpdateFailed, ///< 更新失败
    kErrorPkgUpdateSuccess, ///< 更新成功
    kPkgUpdateing, ///< 更新中
    kErrorPkgKillFailed, ///< kill应用失败
    kErrorPkgKillSuccess, ///< kill成功
    kErrorPkgQuerySuccess, ///< 查询成功
    kErrorPkgQueryFailed ///< 查询失败
};

template<typename T = int>
inline T statuCode(StatusCode statuCode)
{
    return static_cast<T>(statuCode);
}

} // namespace util
} // namespace linglong

#define STATUS_CODE(code) linglong::util::statuCode<int>(linglong::util::StatusCode::code)