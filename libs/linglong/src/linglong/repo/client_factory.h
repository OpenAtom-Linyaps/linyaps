/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

extern "C" {
#include "api/ClientAPI.h"
}

#include "linglong/utils/configure.h"

#include <QObject>
#include <QPoint>

#include <memory>

namespace linglong::repo {

class ClientFactory : public QObject
{
    Q_OBJECT
public:
    explicit ClientFactory(const QString &server);
    explicit ClientFactory(std::string server);

    std::shared_ptr<apiClient_t> createClientV2();
    void setServer(const QString &server);
    void setServer(const std::string &server);

private:
    std::string m_server;
    std::string m_user_agent = "linglong/" LINGLONG_VERSION;
};
} // namespace linglong::repo
