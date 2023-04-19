/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_HTTPCLIENT_H_
#define LINGLONG_SRC_MODULE_UTIL_HTTPCLIENT_H_

#include "module/util/singleton.h"

#include <curl/curl.h>

#include <QString>

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
typedef int (*DOWNLOADCALLBACK)(
        void *client, double dltotal, double dlnow, double ultotal, double ulnow);

typedef struct _downloadRet
{
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
     * 设置Http传输参数
     *
     * @param url: url地址
     */
    void initHttpParam(const char *url);

    bool isFinish;
    DOWNLOADCALLBACK progressFun;
    CURL *curlHandle;
    DownloadRet data;

public:
    /*
     * 向服务器请求指定包名\版本\架构数据
     *
     * @param repoName: 查询仓库名
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param outMsg: 服务端返回的结果
     *
     * @return bool: true:成功 false:失败
     */
    bool queryRemoteApp(const QString &repoName,
                        const QString &pkgName,
                        const QString &pkgVer,
                        const QString &pkgArch,
                        QString &outMsg);

    /*
     * 向服务器请求指定包名\版本\架构数据
     *
     * @param repoName: 查询仓库名
     * @param repoUrl: 查询仓库Url地址
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param outMsg: 服务端返回的结果
     *
     * @return bool: true:成功 false:失败
     */
    bool queryRemoteApp(const QString &repoName,
                        const QString &repoUrl,
                        const QString &pkgName,
                        const QString &pkgVer,
                        const QString &pkgArch,
                        QString &outMsg);

    /*
     * 上传bundle信息
     *
     * @param info: bundle文件信息
     * @param dnsOfLinglong: 玲珑仓库域名地址
     *
     * @return int: kSuccess:成功 kFail:失败
     */
    int pushServerBundleData(const QString &info,
                             const QString &dnsOfLinglong,
                             const QString &token);
};
} // namespace util
} // namespace linglong

#define HTTPCLIENT linglong::util::HttpClient::instance()
#endif
