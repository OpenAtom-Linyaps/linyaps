#pragma once

#include <QDBusArgument> // for QDBusArgument, operator<<
#include <QDBusMetaType>
#include <QLoggingCategory> // for qCCritical
#include <QMap>             // for QMap
#include <QMessageLogger>   // for QMessageLogger
#include <QVariant>         // for QVariant
#include <QVariantMap>      // for QVariantMap

#include "qserializer/core.h" // for qserializer_log, QSERIALIZER_DE...
#include "qserializer/detail/QSerializer.h" // for log

namespace qserializer::detail::dbus
{

template <typename T>
QDBusArgument &move_in(QDBusArgument &args, const T &x)
{
        QVariant v = QVariant::fromValue<T>(x);
        if (v.template canConvert<QVariantMap>()) {
                args << v.toMap();
                return args;
        }
        qCCritical(log) << "Failed to write QDBusArgument as a QVariantMap";
        return args;
}

template <typename T>
const QDBusArgument &move_out(const QDBusArgument &args, T &x)
{
        QVariantMap map;
        args >> map;
        QVariant v = map;
        if (v.template canConvert<T>()) {
                x = v.value<T>();
                return args;
        }
        qCCritical(log) << "Failed to read QDBusArgument as a QVariantMap";
        return args;
}

}
