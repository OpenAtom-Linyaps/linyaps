/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

QNetworkReply *HttpRestClient::doRequest(const QByteArray &verb, QNetworkRequest &request, QIODevice *data,
                                         const QByteArray &bytes)
{
    QEventLoop loop;
    //    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = nullptr;

    if (data != nullptr) {
        reply = networkMgr().sendCustomRequest(request, verb, data);
    } else {
        reply = networkMgr().sendCustomRequest(request, verb, bytes);
    }

    QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    return reply;
}

QNetworkReply *HttpRestClient::post(QNetworkRequest &request, const QByteArray &data)
{
    return doRequest("POST", request, nullptr, data);
}

QNetworkReply *HttpRestClient::del(QNetworkRequest &request)
{
    return doRequest("DELETE", request, nullptr, "");
}

QNetworkReply *HttpRestClient::put(QNetworkRequest &request, const QByteArray &data)
{
    return doRequest("PUT", request, nullptr, data);
}

QNetworkReply *HttpRestClient::get(QNetworkRequest &request)
{
    return doRequest("GET", request, nullptr, "");
}

HttpRestClient::HttpRestClient()
{
    // User-Agent: Mozilla/<version> (<system-information>) <platform> (<platform-details>) <extensions>
    // User-Agent: <product> / <product-version> <comment>
    userAgent = "linglong/1.0.0";
}

} // namespace util
} // namespace linglong