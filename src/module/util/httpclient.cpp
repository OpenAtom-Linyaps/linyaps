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
#include <QEventLoop>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "module/util/file.h"
#include "builder/builder/builder_config.h"

namespace linglong {
namespace util {

// 存储软件包信息服务器配置文件
const QString serverConfigPath = getLinglongRootPath() + "/config.json";

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
    std::string data((const char *)content, (size_t)size * nmemb);

    *((std::stringstream *)stream) << data;

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
bool HttpClient::queryRemoteApp(const QString &pkgName, const QString &pkgVer, const QString &pkgArch, QString &outMsg)
{
    QString configUrl = "";
    int statusCode = linglong::util::getLocalConfig("appDbUrl", configUrl);
    if (STATUS_CODE(kSuccess) != statusCode) {
        return false;
    }

    QString postUrl = "";
    if (configUrl.endsWith("/")) {
        postUrl = configUrl + "apps/fuzzysearchapp";
    } else {
        postUrl = configUrl + "/apps/fuzzysearchapp";
    }

    bool ret = false;
    QJsonObject obj;
    obj["AppId"] = pkgName;
    obj["version"] = pkgVer;
    obj["arch"] = pkgArch;
    const QUrl url(postUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();
    QNetworkAccessManager mgr;
    QNetworkReply *reply = mgr.post(request, data);
    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            outMsg = QString::fromUtf8(reply->readAll());
            ret = true;
        } else {
            qCritical() << reply->errorString();
            reply->abort();
        }
        reply->deleteLater();
        eventLoop.quit();
    });
    // 10s 超时
    QTimer::singleShot(10000, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    return ret;
}

/*
 * 上传文件
 *
 * @param filePath: 文件路径
 * @param dnsOfLinglong: 玲珑仓库域名地址
 * @param flags: bundle：上传bundle文件  ostree： 上传repo仓库
 *
 * @return int: kSuccess:成功 kFail:失败
 */
int HttpClient::uploadFile(const QString &filePath, const QString &dnsOfLinglong, const QString &flags, const QString &token)
{
    std::string urlStr = QString(dnsOfLinglong + "apps/upload").toStdString();
    const char *url = urlStr.c_str();

    initHttpParam(url);

    std::stringstream out;
    curl_httppost *post = nullptr;
    curl_httppost *last = nullptr;

    struct curl_slist *headers = NULL;
    std::string tokenStr = QString("X-Token: " + token).toStdString();
    const char * tokenMsg = tokenStr.c_str();

    headers = curl_slist_append(headers, tokenMsg);
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
    //设置为非0表示本次操作为POST
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
        return STATUS_CODE(kFail);
    }
    int resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
        curl_easy_cleanup(curlHandle);
        curl_global_cleanup();
        return STATUS_CODE(kFail);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();

    auto outMsg = QString::fromStdString(out.str());
    QJsonObject retUploadObject = QJsonDocument::fromJson(outMsg.toUtf8()).object();
    if (retUploadObject["code"].toInt() != 0) {
        qCritical() << "upload file failed: " + filePath;
        qCritical() << retUploadObject["msg"].toString();
        return STATUS_CODE(kFail);
    }
    return STATUS_CODE(kSuccess);
}

/*
 * 上传bundle信息
 *
 * @param info: bundle文件信息
 * @param dnsOfLinglong: 玲珑仓库域名地址
 *
 * @return int: kSuccess:成功 kFail:失败
 */
int HttpClient::pushServerBundleData(const QString &info, const QString &dnsOfLinglong, const QString &token)
{
    std::string urlStr = QString(dnsOfLinglong + "apps").toStdString();
    const char *url = urlStr.c_str();

    initHttpParam(url);

    std::stringstream out;
    // HTTP报文头
    struct curl_slist *headers = NULL;
    std::string tokenStr = QString("X-Token: " + token).toStdString();
    const char * tokenMsg = tokenStr.c_str();

    headers = curl_slist_append(headers, tokenMsg);
    headers = curl_slist_append(headers, "Content-Type: application/json");
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
        return STATUS_CODE(kFail);
    }
    int resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();

    auto outMsg = QString::fromStdString(out.str());
    QJsonObject retUploadObject = QJsonDocument::fromJson(outMsg.toUtf8()).object();
    if (retUploadObject["code"].toInt() != 0) {
        qCritical() << "upload bundle info failed: " + info;
        qCritical() << retUploadObject["msg"].toString();
        return STATUS_CODE(kFail);
    }
    return STATUS_CODE(kSuccess);
}

QString HttpClient::getToken(const QString &dnsOfLinglong, QStringList userInfo)
{
    QString token = "";
    auto username = userInfo.first();
    auto password = userInfo.last();

    QString postUrl = dnsOfLinglong + "auth";
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

    std::string sendString = "{\"username\":\"" + username.toStdString() + "\",\"password\":\"" + password.toStdString() + "\"}";

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
        return token;
    }

    long resCode = 0;
    code = curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode);
    if (code != CURLE_OK || resCode != 200) {
        qCritical() << "curl_easy_getinfo err:" << curl_easy_strerror(code) << ", resCode" << resCode;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curlHandle);
    curl_global_cleanup();

    auto ret = QString::fromStdString(out.str());

    QJsonObject retObject = QJsonDocument::fromJson(ret.toUtf8()).object();
    auto data = retObject["data"].toObject();
    token = data["token"].toString();

    return token;
}

} // namespace util
} // namespace linglong
