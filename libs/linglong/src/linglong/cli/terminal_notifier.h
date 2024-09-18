// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "interactive_notifier.h"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::cli {
class TerminalNotifier final : public InteractiveNotifier
{
public:
    utils::error::Result<api::types::v1::InteractionReply>
    request(const api::types::v1::InteractionRequest &request) override;
    utils::error::Result<void> notify(const api::types::v1::InteractionRequest &request) override;
    ~TerminalNotifier() override = default;
};
} // namespace linglong::cli
