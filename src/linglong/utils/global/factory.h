/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H
#define LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H

#include "linglong/api/v1/dbus/app_manager1.h"
#include "linglong/api/v1/dbus/package_manager1.h"
#include "linglong/package_manager/package_manager.h"

#include <QDBusConnection>
#include <QString>

#include <memory>
#include <mutex>

// move utils
template<typename T>
class Factory
{
public:
    Factory(std::function<std::shared_ptr<T>()> fn)
        : fn(fn)
    {
    }

    std::shared_ptr<T> builder()
    {
        std::lock_guard _(mu);
        if (this->value == nullptr) {
            return this->value = this->fn();
        } else {
            return this->value;
        }
    }

    ~Factory() = default;

private:
    std::mutex mu;
    std::function<std::shared_ptr<T>()> fn;
    std::shared_ptr<T> value;
};

extern template class Factory<linglong::api::v1::dbus::AppManager1>;
extern template class Factory<OrgDeepinLinglongPackageManager1Interface>;
extern template class Factory<linglong::service::PackageManager>;

#endif // LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H