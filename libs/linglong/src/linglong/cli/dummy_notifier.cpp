// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dummy_notifier.h"

namespace linglong::cli {

utils::error::Result<api::types::v1::InteractionReply>
DummyNotifier::request([[maybe_unused]] const api::types::v1::InteractionRequest &request)
{
    return api::types::v1::InteractionReply{ .action = "dummy" };
};

utils::error::Result<void>
DummyNotifier::notify([[maybe_unused]] const api::types::v1::InteractionRequest &request)
{
    return LINGLONG_OK;
}

} // namespace linglong::cli
