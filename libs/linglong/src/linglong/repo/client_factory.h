/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ClientApi.h"

#include <QObject>
#include <QPoint>

namespace linglong::repo {

class ClientFactory : public QObject
{
    Q_OBJECT
public:
    ClientFactory(const QString &server);
    ClientFactory(const std::string &server);

    QSharedPointer<api::client::ClientApi> createClient() const;

    void setServer(QString server);

private:
    QString m_server;
};
} // namespace linglong::repo
