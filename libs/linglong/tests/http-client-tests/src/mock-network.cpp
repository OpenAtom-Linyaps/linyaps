/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mock-network.h"

MockReply::MockReply() { }

void MockReply::JSON(int code, QJsonDocument doc)
{
    setRawHeader("Content-Type", QByteArray("application/json; charset=utf-8"));
    this->Bytes(code, doc.toJson());
}

void MockReply::JSON(int code, QJsonObject obj)
{
    this->JSON(code, QJsonDocument(obj));
}

void MockReply::JSON(int code, QJsonArray arr)
{
    this->JSON(code, QJsonDocument(arr));
}

void MockReply::Bytes(int code, QByteArray data)
{
    if (code >= 400) {
        setError(NetworkError(code), "http code error");
    }
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, code);
    body = new QBuffer(this);
    body->setData(data);
    body->open(ReadOnly);
    setOpenMode(ReadOnly);
    QTimer::singleShot(0, this, &QNetworkReply::finished);
}

void MockReply::abort() { }

qint64 MockReply::readData(char *data, qint64 maxSize)
{
    return body->read(data, maxSize);
}

QNetworkReply *MockQNetworkAccessManager::createRequest(Operation op,
                                                        const QNetworkRequest &req,
                                                        QIODevice *outgoingData)
{
    auto rep = new MockReply();
    Q_EMIT onCreateRequest(rep, op, req, outgoingData);
    return rep;
}