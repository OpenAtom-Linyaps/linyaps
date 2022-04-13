/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang <yuanqiliang@uniontech.com>
 *
 * Maintainer: yuanqiliang <yuanqiliang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>

#include "singleton.h"

namespace linglong {
namespace util {
class LogHandlerPrivate;
class LogHandler
    : public QObject
    , public linglong::util::Singleton<LogHandler>
{
    Q_OBJECT
    friend class linglong::util::Singleton<LogHandler>;

public:
    explicit LogHandler(QObject *parent = nullptr);
    void installMessageHandler(); // 给Qt安装消息处理函数
    void uninstallMessageHandler(); // 取消安装消息处理函数并释放资源

private:
    LogHandlerPrivate *const d_ptr;
    Q_DECLARE_PRIVATE(LogHandler);
};
} // namespace util
} // namespace linglong