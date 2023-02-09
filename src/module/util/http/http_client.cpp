/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "http_client.h"

#include <QEventLoop>
#include <QNetworkAccessManager>

namespace linglong {
namespace util {

QNetworkAccessManager &networkMgr()
{
    static QNetworkAccessManager manager;
    return manager;
}

QNetworkReply *HttpRestClient::doRequest(const QByteArray &verb,
                                         QNetworkRequest &request,
                                         QIODevice *data,
                                         QHttpMultiPart *multiPart,
                                         const QByteArray &bytes)
{
    QEventLoop loop;
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);

    if (multiPart == nullptr) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }

    QNetworkReply *reply = nullptr;

    if (data != nullptr) {
        reply = networkMgr().sendCustomRequest(request, verb, data);
    } else if (multiPart != nullptr) {
        reply = networkMgr().sendCustomRequest(request, verb, multiPart);
    } else {
        reply = networkMgr().sendCustomRequest(request, verb, bytes);
    }

    QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    return reply;
}

QNetworkReply *HttpRestClient::post(QNetworkRequest &request, const QByteArray &data)
{
    return doRequest("POST", request, nullptr, nullptr, data);
}

QNetworkReply *HttpRestClient::del(QNetworkRequest &request)
{
    return doRequest("DELETE", request, nullptr, nullptr, "");
}

QNetworkReply *HttpRestClient::put(QNetworkRequest &request, const QByteArray &data)
{
    return doRequest("PUT", request, nullptr, nullptr, data);
}

QNetworkReply *HttpRestClient::get(QNetworkRequest &request)
{
    return doRequest("GET", request, nullptr, nullptr, "");
}

HttpRestClient::HttpRestClient()
{
    // User-Agent: Mozilla/<version> (<system-information>) <platform> (<platform-details>)
    // <extensions> User-Agent: <product> / <product-version> <comment>
    userAgent = "linglong/1.0.0";
}

QNetworkReply *HttpRestClient::put(QNetworkRequest &request, QHttpMultiPart *multiPart)
{
    return doRequest("PUT", request, nullptr, multiPart, "");
}

} // namespace util
} // namespace linglong