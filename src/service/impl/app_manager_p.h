/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_P_H_
#define LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_P_H_

#include "module/repo/repo.h"
#include "module/runtime/app.h"

namespace linglong {
namespace service {
class AppManager;

class AppManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit AppManagerPrivate(AppManager *parent);
    ~AppManagerPrivate() override = default;

private:
    QMap<QString, QSharedPointer<linglong::runtime::App>> apps = {};
    linglong::repo::Repo *repo = nullptr;

public:
    AppManager *const q_ptr;
    Q_DECLARE_PUBLIC(AppManager);
};
} // namespace service
} // namespace linglong
#endif
