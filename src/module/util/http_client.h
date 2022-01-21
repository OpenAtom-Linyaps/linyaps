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

class HttpClient
{
public:
    //    static QNetworkReply *get(const QNetworkRequest &request);
    //    static QNetworkReply *getFile(const QNetworkRequest &request);
};

} // namespace util
} // namespace linglong
#endif // LINGLONG_SRC_MODULE_UTIL_HTTP_CLIENT_H_
