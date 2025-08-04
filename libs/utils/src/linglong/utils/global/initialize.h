/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QObject>

#include <atomic>

namespace linglong::utils::global {

void applicationInitialize(bool appForceStderrLogging = false);
void installMessageHandler();
bool linglongInstalled();
void cancelAllTask() noexcept;
void initLinyapsLogSystem(const char *command);

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

} // namespace linglong::utils::global
