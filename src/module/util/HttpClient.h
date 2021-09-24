/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
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

#include <curl/curl.h>
#include <QString>
namespace linglong {
namespace util {

/** 
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

class HttpClient
{
private:
    HttpClient()
    {
        mProgressFun = nullptr;
        mIsFinish = false;
        mData = {0};
    }
    int getlock();
    int releaselock(int fd);
    void initHttpParam(const char *url);
    void getFullPath(const char *url, const char *savePath, char *fullPath, int maxSize);
    bool checkPara(const char *url, const char *savePath);
    void showInfo();

    static HttpClient *sInstance;
    bool mIsFinish;
    DOWNLOADCALLBACK mProgressFun;
    CURL *mCurlHandle;
    DownloadRet mData;

public:
    static HttpClient *getInstance();
    static void release();
    bool loadHttpData(const QString qurl, const QString qsavePath);
    void setProgressCallback(DOWNLOADCALLBACK progressFun);
    bool getDownloadInfo(DownloadRet &dataInfo)
    {
        return true;
    }
};
} // namespace util
} // namespace linglong