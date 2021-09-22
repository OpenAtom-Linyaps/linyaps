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

#include "HttpClient.h"

#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

//#include <gio/gio.h>
//#include <glib.h>
//#include<pthread.h>
using namespace std;

namespace linglong {
namespace util {

const char *LINGLONGHTTPCLIENTLOCK = "/tmp/linglongHttpDownload.lock";
HttpClient *HttpClient::sInstance = nullptr;

long getCurrentTime()
{
    struct timeval timeVal;
    gettimeofday(&timeVal, NULL);
    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000;
}

HttpClient *HttpClient::getInstance()
{
    if (sInstance == nullptr) {
        sInstance = new HttpClient();
    }
    return sInstance;
}

void HttpClient::release()
{
    if (sInstance != nullptr) {
        delete sInstance;
        sInstance = nullptr;
    }
}

int HttpClient::getlock()
{
    int fd = -1;
    int ret = -1;
    fd = open(LINGLONGHTTPCLIENTLOCK, O_CREAT | O_TRUNC | O_RDWR, 0777);

    if (fd < 0) {
        fprintf(stdout, "getlock open err:[%d], message:%s\n", errno, strerror(errno));
        return -1;
    }

    ret = flock(fd, LOCK_NB | LOCK_EX);
    if (ret < 0) {
        fprintf(stdout, "getlock flock err:[%d], message:%s\n", errno, strerror(errno));
        close(fd);
        // 确认临时文件是否需要删除
        return -1;
    }
    return fd;
}

int HttpClient::releaselock(int fd)
{
    if (fd == -1) {
        return fd;
    }
    flock(fd, LOCK_UN);
    close(fd);
    unlink(LINGLONGHTTPCLIENTLOCK);
    // 确认临时文件是否需要删除
    return 0;
}

void HttpClient::setProgressCallback(DOWNLOADCALLBACK progressFun)
{
    mProgressFun = progressFun;
}

bool HttpClient::initHttpParam(const char *url)
{
    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    mCurlHandle = curl_easy_init();

    /* set URL to get here */
    curl_easy_setopt(mCurlHandle, CURLOPT_URL, url);

    curl_easy_setopt(mCurlHandle, CURLOPT_TIMEOUT, 3600); // 超时(单位S)

    curl_easy_setopt(mCurlHandle, CURLOPT_LOW_SPEED_LIMIT, 1); //如果15秒内平均速度低于每秒1字节就停止

    curl_easy_setopt(mCurlHandle, CURLOPT_LOW_SPEED_TIME, 15);

    // curl_easy_setopt(mCurlHandle, CURLOPT_HEADER, 1); //下载数据包括HTTP头部
    // 此时若保存文件侧文件数据打不开

    /* Switch on full protocol/debug output while testing */
    // curl_easy_setopt(mCurlHandle, CURLOPT_VERBOSE, 1L);

    /* disable progress meter, set to 0L to enable it */
    if (mProgressFun != nullptr && mProgressFun != NULL) {
        curl_easy_setopt(mCurlHandle, CURLOPT_PROGRESSFUNCTION, mProgressFun);
    }
    curl_easy_setopt(mCurlHandle, CURLOPT_PROGRESSDATA, NULL);
    curl_easy_setopt(mCurlHandle, CURLOPT_NOPROGRESS, 0L);

    /* send all data to this function  */
    //curl_easy_setopt(mCurlHandle, CURLOPT_WRITEFUNCTION, write_data);
}

void HttpClient::getFullPath(const char *url, const char *savePath, char *fullPath, int maxSize)
{
    const char *ptr = url;
    ptr = strrchr(url, '/');
    int len = strlen(savePath);
    strncpy(fullPath, savePath, len);
    if (strlen(ptr) + len < maxSize) {
        if (savePath[len - 1] == '/') {
            strcat(fullPath, ptr + 1);
        } else {
            strcat(fullPath, ptr);
        }
    } else {
        fprintf(stdout, "getFullPath error path is too long\n");
    }
}

void HttpClient::showInfo()
{
    if (!mIsFinish) {
        return;
    }
    cout << "Http Respond Code:" << mData.resCode << endl;
    cout << "Http file size:" << mData.fileSize << endl;
    cout << "Http Content size:" << mData.contentSize << endl;
    cout << "Http Content type:" << mData.contentType << endl;
    cout << "Http total time:" << mData.spendTime << " s" << endl;
    cout << "Http download speed:" << mData.downloadSpeed / 1024 << " kb/s" << endl;
}

bool HttpClient::checkPara(const char *url, const char *savePath)
{
    /* 判断目标文件夹是否存在 不存在则返回*/
    if (access(savePath, F_OK)) {
        return false;
    }
    if (url == NULL) {
        return false;
    }
    if (strncmp(url, "http:", 5) != 0 && strncmp(url, "https:", 6) != 0) {
        return false;
    }

    return true;
}

// https://cdn.zoom.us/prod/5.7.29123.0808/zoom_x86_64.tar.xz
// http://10.20.52.184/hqhdebstore/pool/main/l/llcmd/llcmd_1.0.0-1_amd64.deb
bool HttpClient::loadHttpData(const QString qurl, const QString qsavePath)
{
    std::string strUrl = qurl.toStdString();
    std::string strPath = qsavePath.toStdString();

    const char *url = strUrl.c_str();
    const char *savePath = strPath.c_str();

    if (!checkPara(url, savePath)) {
        return false;
    }
    //加锁
    // pthread_mutex_lock(&mutex);
    int fd = getlock();
    if (fd == -1) {
        fprintf(stdout, "HttpClient loadHttpData is downloading, please wait a moment and retry\n");
        return -1;
    }
    long start = getCurrentTime();
    char fullPath[256] = {'\0'};
    getFullPath(url, savePath, fullPath, 256);
    FILE *pagefile;
    initHttpParam(url);
    /* open the file */
    pagefile = fopen(fullPath, "wb");
    if (pagefile) {
        /* write the page body to this file handle */
        curl_easy_setopt(mCurlHandle, CURLOPT_WRITEDATA, pagefile);

        mIsFinish = false;
        /* get it! */
        CURLcode code = curl_easy_perform(mCurlHandle);
        if (code != CURLE_OK) {
            cout << "curl_easy_perform err code:" << curl_easy_strerror(code) << endl;
            curl_easy_cleanup(mCurlHandle);
            curl_global_cleanup();
            fclose(pagefile);
            //解锁
            // pthread_mutex_unlock(&mutex);
            releaselock(fd);
            return -1;
        }

        long resCode = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_RESPONSE_CODE, &resCode);
        if (code != CURLE_OK) {
            cout << "1.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }
        //获取下载文件的大小 字节
        double fileSize = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_SIZE_DOWNLOAD, &fileSize);
        if (code != CURLE_OK) {
            cout << "2.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }
        //下载内容大小
        double contentSize = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentSize);
        if (code != CURLE_OK) {
            cout << "3.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }
        //下载文件类型 text/html application/x-tar application/x-debian-package image/jpeg
        // Http Header里的Content-Type一般有这三种：
        // application/x-www-form-urlencoded：数据被编码为名称/值对。这是标准的编码格式。
        // multipart/form-data： 数据被编码为一条消息，页上的每个控件对应消息中的一个部分。
        // text/plain
        char *contentType = NULL;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_CONTENT_TYPE, &contentType);
        if (code != CURLE_OK) {
            cout << "4.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }
        //获取下载总耗时包括域名解析、TCP连接
        double spendTime = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_TOTAL_TIME, &spendTime);
        if (code != CURLE_OK) {
            cout << "5.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }

        //下载速度 单位字节
        double downloadSpeed = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_SPEED_DOWNLOAD, &downloadSpeed);
        if (code != CURLE_OK) {
            cout << "6.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
        }

        mData.resCode = resCode;
        mData.fileSize = (long)fileSize;
        mData.contentSize = (long)contentSize;
        mData.contentType = contentType;
        mData.downloadSpeed = downloadSpeed;
        mData.spendTime = spendTime;
        /* close the header file */
        fclose(pagefile);
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(mCurlHandle);
    curl_global_cleanup();
    long end = getCurrentTime();
    cout << "the program spend time is: " << end - start << " ms" << endl;
    //解锁
    // pthread_mutex_unlock(&mutex);
    releaselock(fd);
    mIsFinish = true;
    showInfo();
    return true;
}
} // namespace util
} // namespace linglong
