/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "client_factory.h"

#include "api/ClientAPI.h"

#include <string>

namespace linglong::repo {

ClientFactory::ClientFactory(const QString &server)
    : m_server(server)
{
}

ClientFactory::ClientFactory(const std::string &server)
    : m_server(QString::fromStdString(server))
{
}

std::shared_ptr<apiClient_t> ClientFactory::createClientV2()
{
    auto client = apiClient_create_with_base_path(m_server.toStdString().c_str(), nullptr, nullptr);
    return std::shared_ptr<apiClient_t>(client, apiClient_free);
}

void ClientFactory::setServer(QString server)
{
    m_server = server;
}

void ClientFactory::setServer(std::string server)
{
    m_server = QString::fromStdString(server);
}
} // namespace linglong::repo
