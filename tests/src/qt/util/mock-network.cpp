/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mock-network.h"

BlobReply::BlobReply(int code, QByteArray data, QString type)
{
    setHeader(QNetworkRequest::ContentTypeHeader, type);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, code);
    body = new QBuffer(this);
    body->setData(data);
    body->open(ReadOnly);
    setOpenMode(ReadOnly);
    if (code >= 400) {
        setError(NetworkError(code), "http code error");
    }
}

void BlobReply::abort() { }

qint64 BlobReply::readData(char *data, qint64 maxSize)
{
    return body->read(data, maxSize);
}

void MockQNetworkAccessManager::setStatus(int code)
{
    http_code = code;
}

void MockQNetworkAccessManager::setBody(QJsonDocument doc)
{
    this->http_body = doc;
}

QNetworkReply *MockQNetworkAccessManager::createRequest(Operation op,
                                                        const QNetworkRequest &req,
                                                        QIODevice *outgoingData)
{
    auto rep = new BlobReply(http_code, http_body.toJson());
    QTimer::singleShot(0, rep, &QNetworkReply::finished);
    return rep;
}