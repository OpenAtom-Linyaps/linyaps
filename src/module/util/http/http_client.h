/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_HTTP_CLIENT_H_
#define LINGLONG_SRC_MODULE_UTIL_HTTP_CLIENT_H_

#include <QNetworkReply>
#include <QNetworkRequest>

namespace linglong {
namespace util {

QNetworkAccessManager &networkMgr();

class HttpRestClient
{
public:
    HttpRestClient();

    QNetworkReply *get(QNetworkRequest &request);
    //    QNetworkReply *post(const QNetworkRequest &request, QIODevice *data);
    QNetworkReply *post(QNetworkRequest &request, const QByteArray &data);
    //    QNetworkReply *put(const QNetworkRequest &request, QIODevice *data);
    QNetworkReply *put(QNetworkRequest &request, const QByteArray &data);
    QNetworkReply *put(QNetworkRequest &request, QHttpMultiPart *multiPart);
    QNetworkReply *del(QNetworkRequest &request);

    QString userAgent;

private:
    QNetworkReply *doRequest(const QByteArray &verb, QNetworkRequest &request, QIODevice *data,
                             QHttpMultiPart *multiPart, const QByteArray &bytes);
};

} // namespace util
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_UTIL_HTTP_CLIENT_H_
