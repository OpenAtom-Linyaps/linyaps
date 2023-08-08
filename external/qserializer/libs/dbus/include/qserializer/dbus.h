#pragma once

#include "qserializer/core.h" // for QSERIALIZER_DECLARE, QSERIALIZER_IMPL
#include "qserializer/detail/dbus/operators.h"

#define QSERIALIZER_DECLARE_DBUS_OPERATORS(T)                                  \
        [[maybe_unused]] inline const QDBusArgument &operator<<(               \
                QDBusArgument &args, const QSharedPointer<T> &x)               \
        {                                                                      \
                return qserializer::detail::dbus::move_in<QSharedPointer<T>>(  \
                        args, x);                                              \
        }                                                                      \
        [[maybe_unused]] inline const QDBusArgument &operator>>(               \
                const QDBusArgument &args, QSharedPointer<T> &x)               \
        {                                                                      \
                return qserializer::detail::dbus::move_out<QSharedPointer<T>>( \
                        args, x);                                              \
        }

// NOTE:
// This QSERIALIZER_DECLARE_DBUS and QSERIALIZER_IMPL_DBUS marcos
// should be always used in a global namespace.

#define QSERIALIZER_DECLARE_DBUS(T) \
        QSERIALIZER_DECLARE(T);     \
        QSERIALIZER_DECLARE_DBUS_OPERATORS(T);
#define QSERIALIZER_IMPL_DBUS(T, ...)                                        \
        QSERIALIZER_IMPL(T, {                                                \
                qDBusRegisterMetaType<QSharedPointer<::T>>();                \
                qDBusRegisterMetaType<QList<QSharedPointer<::T>>>();         \
                qDBusRegisterMetaType<QMap<QString, QSharedPointer<::T>>>(); \
        } __VA_OPT__(, ) __VA_ARGS__)
