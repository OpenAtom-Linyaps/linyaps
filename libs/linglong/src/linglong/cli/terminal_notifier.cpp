// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "terminal_notifier.h"

#include <iostream>

namespace linglong::cli {

utils::error::Result<void>
TerminalNotifier::notify(const api::types::v1::InteractionRequest &request)
{
    auto &stdout = std::cout;
    stdout << request.summary << "\n";

    if (request.body) {
        stdout << "  " << request.body.value() << "\n";
    }

    stdout.flush();
    return LINGLONG_OK;
}

utils::error::Result<api::types::v1::InteractionReply>
TerminalNotifier::request(const api::types::v1::InteractionRequest &request)
{
    LINGLONG_TRACE("notify message through terminal")
    std::string msg;
    msg.append(request.summary + "\n");

    if (request.body) {
        msg.append(request.body.value() + "\n");
    }

    bool needReply{ false };
    if (request.actions) {
        if (request.actions->empty()) {
            return LINGLONG_ERR("empty action");
        }

        if (request.actions->size() % 2 != 0) {
            return LINGLONG_ERR("invalid action");
        }

        needReply = true;
        msg.append("available actions:\n");

        for (std::size_t index = 1; index < request.actions->size(); index += 2) {
            msg.append(request.actions.value()[index] + "\t");
        }

        msg.append("\nyour choice:");
    } else {
        msg.append("press enter to continue");
    }
    std::cout << msg << std::endl;

    std::string action;
    std::cin >> action;
    auto ret = api::types::v1::InteractionReply{};
    if (needReply) {
        ret.action = std::move(action);
    }

    return ret;
}

} // namespace linglong::cli
