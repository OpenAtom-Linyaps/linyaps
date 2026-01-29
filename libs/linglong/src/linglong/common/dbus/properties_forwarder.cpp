// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "properties_forwarder.h"

#include "linglong/common/formatter.h"
#include "linglong/utils/log/log.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QMetaProperty>

#include <cstdlib>
#include <utility>

namespace linglong::common::dbus {

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
        LogE("parent is nullptr");
        std::abort();
    }

    const auto *mo = parent->metaObject();
    if (mo == nullptr) {
        LogE("couldn't find metaObject of parent.");
        std::abort();
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

utils::error::Result<void> PropertiesForwarder::forward() noexcept
{
    LINGLONG_TRACE("forward propertiesChanged")
    auto msg = QDBusMessage::createSignal(this->path,
                                          "org.freedesktop.DBus.Properties",
                                          "PropertiesChanged");
    msg.setArguments({ this->interface, this->propCache, QStringList{} });

    if (!connection.send(msg)) {
        return LINGLONG_ERR(
          fmt::format("send dbus message failed: {}", connection.lastError().message()));
    }

    return LINGLONG_OK;
}

void PropertiesForwarder::PropertyChanged()
{
    auto *sender = QObject::sender();
    auto sigIndex = QObject::senderSignalIndex();

    const auto *mo = sender->metaObject();
    if (mo == nullptr) {
        LogE("relay propertiesChanged failed.");
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
        LogE("can't find corresponding property: {}", signature.toStdString());
        return;
    }

    auto propIndex = mo->indexOfProperty(propName.constData());
    auto prop = mo->property(propIndex);
    if (!prop.isReadable()) {
        return;
    }

    propCache.insert(propName, prop.read(sender));
}

} // namespace linglong::common::dbus
