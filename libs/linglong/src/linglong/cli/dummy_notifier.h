// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "interactive_notifier.h"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::cli {

// This notifier is used while making a system image.
// At that point, cli and package manager communicate via socket (without dbus)
// and the stdin, stdout, stderr of cli may be redirected to file.
// we need a dummy notifier to get the cli to continue processing current task
class DummyNotifier final : public InteractiveNotifier
{
public:
    utils::error::Result<api::types::v1::InteractionReply>
    request(const api::types::v1::InteractionRequest &request) override;
    utils::error::Result<void> notify(const api::types::v1::InteractionRequest &request) override;
};

} // namespace linglong::cli
