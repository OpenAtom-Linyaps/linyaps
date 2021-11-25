/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "httpclient.h"

#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <QDebug>

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

/*
 * 获取下文件锁
 *
 * @return int: 文件描述符
 */
int HttpClient::getlock()
{
    int fd = -1;
    int ret = -1;
    fd = open(LINGLONGHTTPCLIENTLOCK, O_CREAT | O_TRUNC | O_RDWR, 0777);

    if (fd < 0) {
        //fprintf(stdout, "getlock open err:[%d], message:%s\n", errno, strerror(errno));
        qInfo() << "getlock open err";
        return -1;
    }

    ret = flock(fd, LOCK_NB | LOCK_EX);
    if (ret < 0) {
        //fprintf(stdout, "getlock flock err:[%d], message:%s\n", errno, strerror(errno));
        qInfo() << "getlock flock err";
        close(fd);
        // 确认临时文件是否需要删除
        return -1;
    }
    return fd;
}

/*
 * 释放文件锁
 *
 * @param fd: 文件描述符
 *
 * @return int: 0:成功 其它:失败
 */
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

/*
 * 设置下载进度回调
 *
 * @param progressFun: 回调函数
 *
 */
void HttpClient::setProgressCallback(DOWNLOADCALLBACK progressFun)
{
    mProgressFun = progressFun;
}

/*
 * 设置Http传输参数
 *
 * @param url: url地址
 */
void HttpClient::initHttpParam(const char *url)
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

/*
 * 获取目标文件保存的全路径
 *
 * @param url: url地址
 * @param savePath: 文件存储路径
 * @param fullPath: 文件保存全路径
 * @param maxSize: 路径长度阈值
 */
void HttpClient::getFullPath(const char *url, const char *savePath, char *fullPath, int maxSize)
{
    const char *ptr = url;
    ptr = strrchr(url, '/');
    int len = strlen(savePath);
    int ptrlen = strlen(ptr);
    strncpy(fullPath, savePath, len);
    if (ptrlen + len < maxSize) {
        if (savePath[len - 1] == '/') {
            strcat(fullPath, ptr + 1);
        } else {
            strcat(fullPath, ptr);
        }
    } else {
        //fprintf(stdout, "getFullPath error path is too long\n");
        qInfo() << "getFullPath error path is too long";
    }
}

/*
 * 显示结果信息
 */
void HttpClient::showInfo()
{
    if (!mIsFinish) {
        return;
    }
    qInfo() << "Http Respond Code:" << mData.resCode;
    qInfo() << "Http file size:" << mData.fileSize;
    qInfo() << "Http Content size:" << mData.contentSize;
    qInfo() << "Http total time:" << mData.spendTime << " s";
    qInfo() << "Http download speed:" << mData.downloadSpeed / 1024 << " kb/s";
}

/*
 * 校验Http传输参数
 *
 * @param url: url地址
 * @param savePath: 文件保存地址
 *
 * @return bool: true:成功 false:失败
 */
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

/*
 * 下载文件
 *
 * @param qurl: 目标文件url
 * @param qsavePath: 保存路径
 *
 * @return bool: true:成功 false:失败
 */
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
        //fprintf(stdout, "HttpClient loadHttpData is downloading, please wait a moment and retry\n");
        qInfo() << "HttpClient loadHttpData is downloading, please wait a moment and retry";
        return false;
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
            //cout << "curl_easy_perform err code:" << curl_easy_strerror(code) << endl;
            qInfo() << "curl_easy_perform err code:" << curl_easy_strerror(code);
            curl_easy_cleanup(mCurlHandle);
            curl_global_cleanup();
            fclose(pagefile);
            //解锁
            // pthread_mutex_unlock(&mutex);
            releaselock(fd);
            return false;
        }

        long resCode = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_RESPONSE_CODE, &resCode);
        if (code != CURLE_OK) {
            //cout << "1.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "1.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        //获取下载文件的大小 字节
        double fileSize = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_SIZE_DOWNLOAD, &fileSize);
        if (code != CURLE_OK) {
            //cout << "2.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "2.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        //下载内容大小
        double contentSize = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentSize);
        if (code != CURLE_OK) {
            //cout << "3.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "3.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        //下载文件类型 text/html application/x-tar application/x-debian-package image/jpeg
        // Http Header里的Content-Type一般有这三种：
        // application/x-www-form-urlencoded：数据被编码为名称/值对。这是标准的编码格式。
        // multipart/form-data： 数据被编码为一条消息，页上的每个控件对应消息中的一个部分。
        // text/plain
        char *contentType = NULL;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_CONTENT_TYPE, &contentType);
        if (code != CURLE_OK) {
            //cout << "4.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "4.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        //获取下载总耗时包括域名解析、TCP连接
        double spendTime = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_TOTAL_TIME, &spendTime);
        if (code != CURLE_OK) {
            //cout << "5.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "5.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }

        //下载速度 单位字节
        double downloadSpeed = 0;
        code = curl_easy_getinfo(mCurlHandle, CURLINFO_SPEED_DOWNLOAD, &downloadSpeed);
        if (code != CURLE_OK) {
            //cout << "6.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "6.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }

        mData.resCode = resCode;
        mData.fileSize = (long)fileSize;
        mData.contentSize = (long)contentSize;
        //mData.contentType = contentType;
        mData.downloadSpeed = downloadSpeed;
        mData.spendTime = spendTime;
        /* close the header file */
        fclose(pagefile);
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(mCurlHandle);
    curl_global_cleanup();
    long end = getCurrentTime();
    //cout << "the program spend time is: " << end - start << " ms" << endl;
    qInfo() << "the program spend time is: " << end - start << " ms";
    //解锁
    // pthread_mutex_unlock(&mutex);
    releaselock(fd);
    if (mData.resCode != 200) {
        //删除空文件 to do
        return false;
    }
    mIsFinish = true;
    showInfo();
    return true;
}
} // namespace util
} // namespace linglong