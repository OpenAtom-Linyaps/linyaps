/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#ifndef LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_
#define LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_

#include <QBuffer>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>

class BlobReply : public QNetworkReply
{
    Q_OBJECT
    QBuffer *body;

public:
    BlobReply(int code, QByteArray data, QString type = "application/json; charset=utf-8");

    void abort();

protected:
    qint64 readData(char *data, qint64 maxSize);
};

class MockQNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
    int http_code;
    QJsonDocument http_body;

public:
    void setStatus(int code);

    void setBody(QJsonDocument doc);

protected:
    QNetworkReply *createRequest(Operation op,
                                 const QNetworkRequest &req,
                                 QIODevice *outgoingData = Q_NULLPTR);
};

#endif // LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_