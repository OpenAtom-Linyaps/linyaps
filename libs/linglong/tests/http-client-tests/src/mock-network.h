/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#ifndef LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_
#define LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_

#include <QBuffer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>

class MockReply : public QNetworkReply
{
    Q_OBJECT
    QBuffer *body;

public:
    MockReply();

    void JSON(int code, QJsonArray doc);
    void JSON(int code, QJsonObject doc);
    void JSON(int code, QJsonDocument doc);
    void Bytes(int code, QByteArray data);

    void abort();

protected:
    qint64 readData(char *data, qint64 maxSize);
};

class MockQNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

Q_SIGNALS:
    void onCreateRequest(MockReply *reply,
                         Operation operation,
                         const QNetworkRequest &req,
                         QIODevice *outgoingData);

protected:
    QNetworkReply *createRequest(Operation op,
                                 const QNetworkRequest &req,
                                 QIODevice *outgoingData = Q_NULLPTR);
};

#endif // LINGLONG_TESTS_QT_UTILS_MOCK_NETWORK_H_