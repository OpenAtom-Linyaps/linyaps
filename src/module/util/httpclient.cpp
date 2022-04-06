/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
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
#include <sstream>

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "module/util/fs.h"
#include "service/impl/dbus_retcode.h"

using namespace std;
using namespace linglong::Status;

namespace linglong {
namespace util {

const char *pkgDownloadLock = "/tmp/linglongHttpDownload.lock";

// 存储软件包信息服务器配置文件
const QString serverConfigPath = "/deepin/linglong/config/config";

long getCurrentTime()
{
    struct timeval timeVal;
    gettimeofday(&timeVal, NULL);
    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000;
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

    fd = open(pkgDownloadLock, O_CREAT | O_TRUNC | O_RDWR, 0777);
    if (fd < 0) {
        // fprintf(stdout, "getlock open err:[%d], message:%s\n", errno, strerror(errno));
        qInfo() << "getlock open err";
        return -1;
    }

    ret = flock(fd, LOCK_NB | LOCK_EX);
    if (ret < 0) {
        // fprintf(stdout, "getlock flock err:[%d], message:%s\n", errno, strerror(errno));
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
    unlink(pkgDownloadLock);
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
    this->progressFun = progressFun;
}

/*
 * 设置Http传输参数
 *
 * @param url: url地址
 */
void HttpClient::initHttpParam(const char *url)
{
    isFinish = false;

    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curlHandle = curl_easy_init();

    /* set URL to get here */
    curl_easy_setopt(curlHandle, CURLOPT_URL, url);

    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 3600); // 超时(单位S)
    curl_easy_setopt(curlHandle, CURLOPT_CONNECTTIMEOUT, 3600); // 超时(单位S)

    // curl_easy_setopt(curlHandle, CURLOPT_HEADER, 1); //下载数据包括HTTP头部
    // 此时若保存文件侧文件数据打不开

    /* Switch on full protocol/debug output while testing */
    // curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L);

    /* disable progress meter, set to 0L to enable it */
    if (progressFun != nullptr && progressFun != NULL) {
        curl_easy_setopt(curlHandle, CURLOPT_PROGRESSFUNCTION, progressFun);
    }
    curl_easy_setopt(curlHandle, CURLOPT_PROGRESSDATA, NULL);
    curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 0L);

    /* send all data to this function  */
    // curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
}

/*
 * 从配置文件获取服务器配置参数
 *
 * @param key: 参数名称
 * @param value: 查询结果
 *
 * @return int: 0:成功 其它:失败
 */
int HttpClient::getLocalConfig(const QString &key, QString &value)
{
    if (!linglong::util::fileExists(serverConfigPath)) {
        qCritical() << serverConfigPath << " not exist";
        return StatusCode::FAIL;
    }
    QFile dbFile(serverConfigPath);
    dbFile.open(QIODevice::ReadOnly);
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (QJsonParseError::NoError != parseJsonErr.error) {
        qCritical() << "parse linglong config file err";
        return StatusCode::FAIL;
    }
    QJsonObject dataObject = document.object();
    if (!dataObject.contains(key)) {
        qWarning() << "key:" << key << " not found in config";
        return StatusCode::FAIL;
    }
    value = dataObject[key].toString();
    return StatusCode::SUCCESS;
}

/*
 * 发送数据回调函数
 *
 * @param content: 返回数据指针
 * @param size: 数据块大小
 * @param nmemb: 数据块个数
 * @param stream: 字符串流
 *
 * @return size_t: 写入数据size
 */
size_t writeData(void *content, size_t size, size_t nmemb, void *stream)
{
    string data((const char *)content, (size_t)size * nmemb);

    *((stringstream *)stream) << data << endl;

    return size * nmemb;
}

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
bool HttpClient::queryRemote(const QString &pkgName, const QString &pkgVer, const QString &pkgArch, QString &outMsg)
{
    // curl --location --request POST 'https://linglong-api-dev.deepin.com/apps/fuzzysearchapp' --header 'Content-Type:
    // application/json' --data '{"AppId":"org.deepin.calculator","version":"5.5.23"}'
    QString configUrl = "";
    int statusCode = getLocalConfig("appDbUrl", configUrl);
    if (StatusCode::SUCCESS != statusCode) {
        return false;
    }
    qDebug() << "queryRemote configUrl:" << configUrl;

    QString postUrl = "";
    if (configUrl.endsWith("/")) {
        postUrl = configUrl + "apps/fuzzysearchapp";
    } else {
        postUrl = configUrl + "/apps/fuzzysearchapp";
    }
    int fd = getlock();
    if (fd == -1) {
        qInfo() << "HttpClient queryRemote app data is ongoing, please wait a moment and retry";
        return false;
    }
    std::string url = postUrl.toStdString();
    initHttpParam(url.c_str());
    std::stringstream out;
    // HTTP报文头
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type:application/json");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
    // 设置为非0表示本次操作为POST
    curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
    // --location
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);

    std::string sendString = "{\"AppId\":\"" + pkgName.toStdString() + "\"}";
    if (!pkgVer.isEmpty() || !pkgArch.isEmpty()) {
        sendString = "{\"AppId\":\"" + pkgName.toStdString() + "\",\"version\":\"" + pkgVer.toStdString()
                     + "\",\"arch\":\"" + pkgArch.toStdString() + "\"}";
    }

    // 设置要POST的JSON数据
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, sendString.c_str());

    // 设置接收数据的处理函数和存放变量
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
    // 设置写数据
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);

    CURLcode code = curl_easy_perform(curlHandle);
    if (code != CURLE_OK) {
        qCritical() << "curl_easy_perform err code:" << curl_easy_strerror(code);
        curl_easy_cleanup(curlHandle);
        curl_global_cleanup();
        releaselock(fd);
        return false;
    }

    long resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();
    releaselock(fd);

    outMsg = QString::fromStdString(out.str());
    // 返回请求值
    // string str_json = out.str();
    // printf("\nrequestServerData receive:\n%s\n", str_json.c_str());
    return true;
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
        // fprintf(stdout, "getFullPath error path is too long\n");
        qInfo() << "getFullPath error path is too long";
    }
}

/*
 * 显示结果信息
 */
void HttpClient::showInfo()
{
    if (!isFinish) {
        return;
    }
    qInfo() << "Http Respond Code:" << data.resCode;
    qInfo() << "Http file size:" << data.fileSize;
    qInfo() << "Http Content size:" << data.contentSize;
    qInfo() << "Http total time:" << data.spendTime << " s";
    qInfo() << "Http download speed:" << data.downloadSpeed / 1024 << " kb/s";
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
    // pthread_mutex_lock(&mutex);
    int fd = getlock();
    if (fd == -1) {
        // fprintf(stdout, "HttpClient loadHttpData is downloading, please wait a moment and retry\n");
        qInfo() << "HttpClient loadHttpData is downloading, please wait a moment and retry";
        return false;
    }
    long start = getCurrentTime();
    char fullPath[256] = {'\0'};
    getFullPath(url, savePath, fullPath, 256);
    FILE *pagefile;
    initHttpParam(url);
    pagefile = fopen(fullPath, "wb");
    if (pagefile) {
        /* write the page body to this file handle */
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pagefile);

        isFinish = false;
        CURLcode code = curl_easy_perform(curlHandle);
        if (code != CURLE_OK) {
            // cout << "curl_easy_perform err code:" << curl_easy_strerror(code) << endl;
            qInfo() << "curl_easy_perform err code:" << curl_easy_strerror(code);
            curl_easy_cleanup(curlHandle);
            curl_global_cleanup();
            fclose(pagefile);
            // pthread_mutex_unlock(&mutex);
            releaselock(fd);
            return false;
        }

        long resCode = 0;
        code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
        if (code != CURLE_OK) {
            // cout << "1.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "1.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        // 获取下载文件的大小 字节
        double fileSize = 0;
        code = curl_easy_getinfo(curlHandle, CURLINFO_SIZE_DOWNLOAD, &fileSize);
        if (code != CURLE_OK) {
            // cout << "2.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "2.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        // 下载内容大小
        double contentSize = 0;
        code = curl_easy_getinfo(curlHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentSize);
        if (code != CURLE_OK) {
            // cout << "3.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "3.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        // 下载文件类型 text/html application/x-tar application/x-debian-package image/jpeg
        // Http Header里的Content-Type一般有这三种：
        // application/x-www-form-urlencoded：数据被编码为名称/值对。这是标准的编码格式。
        // multipart/form-data： 数据被编码为一条消息，页上的每个控件对应消息中的一个部分。
        // text/plain
        char *contentType = NULL;
        code = curl_easy_getinfo(curlHandle, CURLINFO_CONTENT_TYPE, &contentType);
        if (code != CURLE_OK) {
            // cout << "4.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "4.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }
        // 获取下载总耗时包括域名解析、TCP连接
        double spendTime = 0;
        code = curl_easy_getinfo(curlHandle, CURLINFO_TOTAL_TIME, &spendTime);
        if (code != CURLE_OK) {
            // cout << "5.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "5.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }

        // 下载速度 单位字节
        double downloadSpeed = 0;
        code = curl_easy_getinfo(curlHandle, CURLINFO_SPEED_DOWNLOAD, &downloadSpeed);
        if (code != CURLE_OK) {
            // cout << "6.curl_easy_getinfo err:" << curl_easy_strerror(code) << endl;
            qInfo() << "6.curl_easy_getinfo err:" << curl_easy_strerror(code);
        }

        data.resCode = resCode;
        data.fileSize = (long)fileSize;
        data.contentSize = (long)contentSize;
        // data.contentType = contentType;
        data.downloadSpeed = downloadSpeed;
        data.spendTime = spendTime;
        /* close the header file */
        fclose(pagefile);
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();
    long end = getCurrentTime();
    // cout << "the program spend time is: " << end - start << " ms" << endl;
    qInfo() << "the program spend time is: " << end - start << " ms";
    // pthread_mutex_unlock(&mutex);
    releaselock(fd);
    if (data.resCode != 200) {
        return false;
    }
    isFinish = true;
    showInfo();
    return true;
}

/*
 * 上传文件
 *
 * @param filePath: 文件路径
 * @param dnsOfLinglong: 玲珑仓库域名地址
 * @param flags: bundle：上传bundle文件  ostree： 上传repo仓库
 *
 * @return int: SUCCESS:成功 FAIL:失败
 */
int HttpClient::uploadFile(const QString &filePath, const QString &dnsOfLinglong, const QString &flags)
{
    QString dnsUrl = dnsOfLinglong + "apps/upload";
    QByteArray dnsUrlByteArr = dnsUrl.toLocal8Bit();
    const char *url = dnsUrlByteArr.data();
    int fd = getlock();
    if (fd == -1) {
        qCritical() << "HttpClient requestServerData is doing, please wait a moment and retry";
        return Status::StatusCode::FAIL;
    }
    initHttpParam(url);
    std::stringstream out;
    curl_httppost *post = nullptr;
    curl_httppost *last = nullptr;

    // 设置为非0表示本次操作为POST
    curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
    // --location
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);

    std::string filePathString = filePath.toStdString();

    curl_formadd(&post, &last, CURLFORM_PTRNAME, "uploadSubPath", CURLFORM_PTRCONTENTS, flags.toStdString().c_str(),
                 CURLFORM_END);
    curl_formadd(&post, &last, CURLFORM_PTRNAME, "file", CURLFORM_FILE, filePathString.c_str(), CURLFORM_END);

    curl_easy_setopt(curlHandle, CURLOPT_HTTPPOST, post);
    // 设置接收数据的处理函数和存放变量
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
    // 设置写数据
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);

    CURLcode code = curl_easy_perform(curlHandle);
    if (code != CURLE_OK) {
        qCritical() << "curl_easy_perform err code:" << curl_easy_strerror(code);
        curl_easy_cleanup(curlHandle);
        curl_global_cleanup();
        releaselock(fd);
        return Status::StatusCode::FAIL;
    }
    int resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
        curl_easy_cleanup(curlHandle);
        curl_global_cleanup();
        releaselock(fd);
        return Status::StatusCode::FAIL;
    }
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();
    releaselock(fd);

    auto outMsg = QString::fromStdString(out.str());
    QJsonObject retUploadObject = QJsonDocument::fromJson(outMsg.toUtf8()).object();
    if (retUploadObject["code"].toInt() != 0) {
        qCritical() << "upload file failed: " + filePath;
        qCritical() << retUploadObject["msg"].toString();
        return Status::StatusCode::FAIL;
    }
    return Status::StatusCode::SUCCESS;
}

/*
 * 上传bundle信息
 *
 * @param info: bundle文件信息
 * @param dnsOfLinglong: 玲珑仓库域名地址
 *
 * @return int: SUCCESS:成功 FAIL:失败
 */
int HttpClient::pushServerBundleData(const QString &info, const QString &dnsOfLinglong)
{
    QString dnsUrl = dnsOfLinglong + "apps";
    QByteArray dnsUrlByteArr = dnsUrl.toLocal8Bit();
    const char *url = dnsUrlByteArr.data();
    int fd = getlock();
    if (fd == -1) {
        qCritical() << "HttpClient requestServerData is doing, please wait a moment and retry";
        return Status::StatusCode::FAIL;
    }
    initHttpParam(url);
    std::stringstream out;
    // HTTP报文头
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type:application/json");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
    // 设置为非0表示本次操作为POST
    curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
    // --location
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);

    std::string sendString = info.toStdString();

    // printf("\nrequestServerData sendString:%s\n", sendString.c_str());
    // 设置要POST的JSON数据
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, sendString.c_str());

    // 设置接收数据的处理函数和存放变量
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
    // 设置写数据
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);

    CURLcode code = curl_easy_perform(curlHandle);
    if (code != CURLE_OK) {
        // cout << "curl_easy_perform err code:" << curl_easy_strerror(code) << endl;
        qCritical() << "curl_easy_perform err code:" << curl_easy_strerror(code);
        curl_easy_cleanup(curlHandle);
        curl_global_cleanup();
        releaselock(fd);
        return Status::StatusCode::FAIL;
    }
    int resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();
    releaselock(fd);

    auto outMsg = QString::fromStdString(out.str());
    QJsonObject retUploadObject = QJsonDocument::fromJson(outMsg.toUtf8()).object();
    if (retUploadObject["code"].toInt() != 0) {
        qCritical() << "upload bundle info failed: " + info;
        qCritical() << retUploadObject["msg"].toString();
        return Status::StatusCode::FAIL;
    }
    return Status::StatusCode::SUCCESS;
}
} // namespace util
} // namespace linglong