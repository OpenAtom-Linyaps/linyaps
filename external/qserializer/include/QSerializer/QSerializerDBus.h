#pragma once

#include <QDBusMetaType>
#include <QSerializer/QSerializer.h>

namespace qserializer_dbus
{

template <typename T>
QDBusArgument &move_in(QDBusArgument &args, const T &x)
{
        QVariant v = QVariant::fromValue<T>(x);
        if (v.template canConvert<QVariantMap>()) {
                args << v.toMap();
                return args;
        }
        qCCritical(qserializer_log)
                << "Failed to write QDBusArgument as a QVariantMap";
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
        qCCritical(qserializer_log)
                << "Failed to read QDBusArgument as a QVariantMap";
        return args;
}

}

#define QSERIALIZER_DECLARE_DBUS_OPERATORS(T)                                  \
        [[maybe_unused]] inline const QDBusArgument &operator<<(               \
                QDBusArgument &args, const QSharedPointer<T> &x)               \
        {                                                                      \
                return qserializer_dbus::move_in<QSharedPointer<T>>(args, x);  \
        }                                                                      \
        [[maybe_unused]] inline const QDBusArgument &operator>>(               \
                const QDBusArgument &args, QSharedPointer<T> &x)               \
        {                                                                      \
                return qserializer_dbus::move_out<QSharedPointer<T>>(args, x); \
        }

#define QSERIALIZER_DECLARE_DBUS(T) \
        QSERIALIZER_DECLARE(T);     \
        QSERIALIZER_DECLARE_DBUS_OPERATORS(T);
#define QSERIALIZER_IMPL_DBUS(T, ...)                                      \
        QSERIALIZER_IMPL(T, {                                              \
                qDBusRegisterMetaType<QSharedPointer<T>>();                \
                qDBusRegisterMetaType<QList<QSharedPointer<T>>>();         \
                qDBusRegisterMetaType<QMap<QString, QSharedPointer<T>>>(); \
        } __VA_OPT__(, ) __VA_ARGS__)
