/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_LOG_LOG_HANDLER_H_
#define LINGLONG_SRC_MODULE_UTIL_LOG_LOG_HANDLER_H_

#include <QObject>

#include "module/util/singleton.h"

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
    ~LogHandler() override;
    void installMessageHandler(); // 给Qt安装消息处理函数
    void uninstallMessageHandler(); // 取消安装消息处理函数并释放资源

private:
    QScopedPointer<LogHandlerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), LogHandler)
};
} // namespace util
} // namespace linglong

#define LOG_HANDLER linglong::util::LogHandler::instance()
#endif