#pragma once

#include <QtGlobal> // for QT_VERSION, QT_VERSION_CHECK

#include "qserializer/detail/QSerializer.h"

#if QT_VERSION <= QT_VERSION_CHECK(6, 2, 0)
#define QSERIALIZER_DECLARE_SHAREDPOINTER_METATYPE(x) \
        Q_DECLARE_METATYPE(QSharedPointer<x>);        \
        Q_DECLARE_METATYPE(QSharedPointer<const x>);
#else
#define QSERIALIZER_DECLARE_SHAREDPOINTER_METATYPE(x) \
        Q_DECLARE_METATYPE(QSharedPointer<const x>);
#endif

// NOTE:
// This QSERIALIZER_DECLARE and QSERIALIZER_IMPL marcos
// should be always used in a global namespace.

#define QSERIALIZER_DECLARE(T)                         \
        QSERIALIZER_DECLARE_SHAREDPOINTER_METATYPE(T); \
        namespace qserializer::detail::init::T         \
        {                                              \
        char init() noexcept;                          \
        static char _ = init();                        \
        };

#define QSERIALIZER_IMPL(T, ...)                                      \
        namespace qserializer::detail::init::T                        \
        {                                                             \
        char init() noexcept                                          \
        {                                                             \
                static char _ = []() -> char {                        \
                        QSerializer<::T>::registerConverters();       \
                        QSerializer<const ::T>::registerConverters(); \
                        __VA_ARGS__;                                  \
                        return 0;                                     \
                }();                                                  \
                return _;                                             \
        }                                                             \
        }
