/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "client_factory.h"

namespace linglong::repo {

ClientFactory::ClientFactory(const QString &server)
    : m_server(server)
{
}

ClientFactory::ClientFactory(const std::string &server)
    : m_server(QString::fromStdString(server))
{
}

QSharedPointer<api::client::ClientApi> ClientFactory::createClient() const
{
    auto api = QSharedPointer<linglong::api::client::ClientApi>::create();
    api->setTimeOut(5000);
    api->setNewServerForAllOperations(m_server);
    api->setParent(QCoreApplication::instance());
    return api;
}

void ClientFactory::setServer(QString server)
{
    m_server = server;
}

} // namespace linglong::repo
