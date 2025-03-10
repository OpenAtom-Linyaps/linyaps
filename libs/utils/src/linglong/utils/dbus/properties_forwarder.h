// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>

namespace linglong::utils::dbus {

class PropertiesForwarder : public QObject
{
    Q_OBJECT
public:
    explicit PropertiesForwarder(QDBusConnection connection,
                                 QString path,
                                 QString interface,
                                 QObject *parent);
    PropertiesForwarder(PropertiesForwarder &&) = delete;
    PropertiesForwarder &operator=(PropertiesForwarder &&) = delete;

    error::Result<void> forward() noexcept;

public Q_SLOTS:
    void PropertyChanged();

private:
    QString path;
    QString interface;
    QDBusConnection connection;
    QVariantMap propCache;
};

} // namespace linglong::utils::dbus
