// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::cli {
class InteractiveNotifier
{
public:
    virtual ~InteractiveNotifier() = default;

    // sending a request to user and waiting for user reply
    // this method MUST be blocking
    virtual utils::error::Result<api::types::v1::InteractionReply>
    request(const api::types::v1::InteractionRequest &request) = 0;

    // sending a message to user
    // this method MUST be non-blocking
    virtual utils::error::Result<void>
    notify(const api::types::v1::InteractionRequest &request) = 0;
};
} // namespace linglong::cli
