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
    : m_server(server.toStdString())
{
}

ClientFactory::ClientFactory(const std::string &server)
    : m_server(server)
{
}

std::shared_ptr<apiClient_t> ClientFactory::createClientV2()
{
    auto client = apiClient_create_with_base_path(m_server.c_str(), nullptr, nullptr);
    client->userAgent = m_user_agent.c_str();
    return std::shared_ptr<apiClient_t>(client, apiClient_free);
}

void ClientFactory::setServer(const QString &server)
{
    m_server = server.toStdString();
}

void ClientFactory::setServer(const std::string &server)
{
    m_server = server;
}
} // namespace linglong::repo
