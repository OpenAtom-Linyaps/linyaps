/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

extern "C" {
#include "api/ClientAPI.h"
}

#include <QObject>
#include <QPoint>

#include <memory>

namespace linglong::repo {

class ClientFactory : public QObject
{
    Q_OBJECT
public:
    ClientFactory(const QString &server);
    ClientFactory(const std::string &server);

    std::shared_ptr<apiClient_t> createClientV2();
    void setServer(QString server);
    void setServer(std::string server);

private:
    QString m_server;
};
} // namespace linglong::repo
