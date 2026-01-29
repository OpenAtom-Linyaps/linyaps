/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/log/log.h"

#include <QObject>

#include <atomic>

namespace linglong::common::global {

void applicationInitialize();
bool linglongInstalled();
void initLinyapsLogSystem(linglong::utils::log::LogBackend backend);

class GlobalTaskControl : public QObject
{
    Q_OBJECT
public:
    static void cancel();
    static bool canceled();
    static const GlobalTaskControl *instance();
Q_SIGNALS:
    void OnCancel();

private:
    std::atomic<bool> cancelFlag = false;
};

} // namespace linglong::common::global
