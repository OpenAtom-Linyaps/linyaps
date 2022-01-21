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

// QNetworkReply *HttpClient::get(const QNetworkRequest &request)
//{
//     QEventLoop loop;
//     QNetworkReply *reply = networkMgr().get(request);
//     QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
//     loop.exec();
//     return reply;
// }

} // namespace util
} // namespace linglong