/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <curl/curl.h>
#include <QString>
#include "module/util/singleton.h"
#include "module/util/status_code.h"

namespace linglong {
namespace util {

/*
 * http下载进度回调函数
 *
 * @param client: 客户端传递给libcurl的数据
 * @param dltotal: 需要下载的总字节数
 * @param dlnow: 已经下载的字节数
 * @param ultotal: 将要上传的字节数
 * @param ulnow: 已经上传的字节数
 *
 * @return int: CURLE_OK(0)
 */
typedef int (*DOWNLOADCALLBACK)(void *client, double dltotal, double dlnow, double ultotal, double ulnow);

typedef struct _downloadRet {
    long resCode;
    long fileSize;
    long contentSize;
    std::string contentType;
    double spendTime;
    double downloadSpeed;

} DownloadRet;

class HttpClient : public linglong::util::Singleton<HttpClient>
{
private:
    /*
     * 获取文件锁
     *
     * @return int: 文件描述符
     */
    int getlock();

    /*
     * 释放文件锁
     *
     * @param fd: 文件描述符
     *
     * @return int: 0:成功 其它:失败
     */
    int releaselock(int fd);

    /*
     * 设置Http传输参数
     *
     * @param url: url地址
     */
    void initHttpParam(const char *url);

    /*
     * 获取目标文件保存的全路径
     *
     * @param url: url地址
     * @param savePath: 文件存储路径
     * @param fullPath: 文件保存全路径
     * @param maxSize: 路径长度阈值
     */
    void getFullPath(const char *url, const char *savePath, char *fullPath, int maxSize);

    /*
     * 校验Http传输参数
     *
     * @param url: url地址
     * @param savePath: 文件保存地址
     *
     * @return bool: true:成功 false:失败
     */
    bool checkPara(const char *url, const char *savePath);

    /*
     * 显示结果信息
     */
    void showInfo();

    bool isFinish;
    DOWNLOADCALLBACK progressFun;
    CURL *curlHandle;
    DownloadRet data;

public:
    /*
     * 向服务器请求指定包名\版本\架构数据
     *
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param outMsg: 服务端返回的结果
     *
     * @return bool: true:成功 false:失败
     */
    bool queryRemoteApp(const QString &pkgName, const QString &pkgVer, const QString &pkgArch, QString &outMsg);

    /*
     * 上传文件
     *
     * @param filePath: 文件路径
     * @param dnsOfLinglong: 玲珑仓库域名地址
     * @param flags: bundle：上传bundle文件  ostree： 上传repo仓库
     *
     * @return int: kSuccess:成功 kFail:失败
     */
    int uploadFile(const QString &filePath, const QString &dnsOfLinglong, const QString &flags, const QString &token);

    /*
     * 上传bundle信息
     *
     * @param info: bundle文件信息
     * @param dnsOfLinglong: 玲珑仓库域名地址
     *
     * @return int: kSuccess:成功 kFail:失败
     */
    int pushServerBundleData(const QString &info, const QString &dnsOfLinglong, const QString &token);

    /*
     * 获取服务器token
     *
     * @param dnsOfLinglong: 玲珑仓库域名地址
     * @param userInfo: 账户信息
     *
     * @return QString: token数据
     */
    QString getToken(const QString &dnsOfLinglong, QStringList userInfo);
};
} // namespace util
} // namespace linglong

#define HTTPCLIENT linglong::util::HttpClient::instance()