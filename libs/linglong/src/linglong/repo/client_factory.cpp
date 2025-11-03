/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "client_factory.h"

#include "api/ClientAPI.h"

#include <string>

namespace linglong::repo {

ClientFactory::ClientFactory(std::string server)
    : m_server(std::move(server))
{
}

std::unique_ptr<ClientAPIWrapper> ClientFactory::createClientV2()
{
    auto *client = apiClient_create_with_base_path(m_server.c_str(), nullptr, nullptr);
    return std::make_unique<ClientAPIWrapper>(client);
}

} // namespace linglong::repo
