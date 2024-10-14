// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "properties_forwarder.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDebug>
#include <QMetaProperty>

#include <utility>

namespace linglong::utils::dbus {

PropertiesForwarder::PropertiesForwarder(QDBusConnection connection,
                                         QString path,
                                         QString interface,
                                         QObject *parent)
    : QObject(parent)
    , path(std::move(path))
    , interface(std::move(interface))
    , connection(std::move(connection))
{
    if (parent == nullptr) {
        qFatal("parent is nullptr");
    }

    const auto *mo = parent->metaObject();
    if (mo == nullptr) {
        qFatal("couldn't find metaObject of parent.");
    }

    for (auto i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto prop = mo->property(i);
        if (!prop.hasNotifySignal()) {
            continue;
        }

        auto signal = prop.notifySignal();
        auto slot = metaObject()->method(metaObject()->indexOfSlot("PropertyChanged()"));
        QObject::connect(parent, signal, this, slot);
    }
}

error::Result<void> PropertiesForwarder::forward() noexcept
{
    LINGLONG_TRACE("forward propertiesChanged")
    auto msg = QDBusMessage::createSignal(this->path,
                                          "org.freedesktop.DBus.Properties",
                                          "PropertiesChanged");
    msg.setArguments({ this->interface, this->propCache, QStringList{} });

    if (!connection.send(msg)) {
        return LINGLONG_ERR(
          QString{ "send dbus message failed: %1" }.arg(connection.lastError().message()));
    }

    return LINGLONG_OK;
}

void PropertiesForwarder::PropertyChanged()
{
    auto *sender = QObject::sender();
    auto sigIndex = QObject::senderSignalIndex();

    const auto *mo = sender->metaObject();
    if (mo == nullptr) {
        qCritical() << __PRETTY_FUNCTION__ << "relay propertiesChanged failed.";
        return;
    }

    auto sig = mo->method(sigIndex);
    auto signature = sig.methodSignature();

    QByteArray propName;
    for (auto i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto prop = mo->property(i);
        if (!prop.hasNotifySignal()) {
            continue;
        }

        if (prop.notifySignal().methodSignature() == signature) {
            propName = prop.name();
            break;
        }
    }

    if (propName.isEmpty()) {
        qCritical() << "can't find corresponding property:" << signature;
        return;
    }

    auto propIndex = mo->indexOfProperty(propName.constData());
    auto prop = mo->property(propIndex);
    if (!prop.isReadable()) {
        return;
    }

    propCache.insert(propName, prop.read(sender));
}

} // namespace linglong::utils::dbus
