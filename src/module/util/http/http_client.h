/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
    inline static const char *kContentTypeJson = "application/json";
    inline static const char *kContentTypeBinaryStream = "application/octet-stream";

    HttpRestClient();

    QNetworkReply *head(QNetworkRequest &request);
    QNetworkReply *get(QNetworkRequest &request);
    //    QNetworkReply *post(const QNetworkRequest &request, QIODevice *device);
    QNetworkReply *post(QNetworkRequest &request, const QByteArray &data);
    QNetworkReply *put(QNetworkRequest &request, QIODevice *device);
    QNetworkReply *put(QNetworkRequest &request, const QByteArray &data);
    QNetworkReply *put(QNetworkRequest &request, QHttpMultiPart *multiPart);
    QNetworkReply *del(QNetworkRequest &request);

    QString userAgent;

private:
    QNetworkReply *doRequest(const QByteArray &verb,
                             QNetworkRequest &request,
                             QIODevice *data,
                             QHttpMultiPart *multiPart,
                             const QByteArray &bytes);
};

#define NewNetworkError(reply)                   \
    NewError(static_cast<int>(reply->error()),   \
             QString("%1 %2 with %3 failed, %4") \
                     .arg(reply->operation())    \
                     .arg(reply->error())        \
                     .arg(reply->request().url().toString(), QString(reply->readAll())))

#define WarpNetworkError(reply) (reply->error() ? NewNetworkError(reply) : Success())

} // namespace util
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_UTIL_HTTP_CLIENT_H_
