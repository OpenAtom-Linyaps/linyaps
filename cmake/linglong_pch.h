#pragma once

// Only precompile nlohmann/json for targets that can find it on their
// include path (guarded instead of unconditional so non-json targets
// don't drag it into their PCH).
#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#endif

// Qt module headers are only pulled in for targets that actually link
// the corresponding Qt module; the QT_*_LIB macros are defined by Qt's
// own CMake config when a target links Qt6::Core / Qt6::DBus.
#ifdef QT_CORE_LIB
#include <QtCore/QtCore>
#endif

#ifdef QT_DBUS_LIB
#include <QtDBus/QtDBus>
#endif
