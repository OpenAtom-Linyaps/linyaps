/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_ERROR_H_
#define LINGLONG_SRC_MODULE_UTIL_ERROR_H_

#include <QMessageLogContext>
#include <QSharedPointer>

class QString;
class QDebug;

namespace linglong {
namespace util {

/*!
 * WARNING:
 * DO NOT USE THIS CLASS IN NEW CODE
 * Use linglong::utils::error::Result
 *
 * Error is a stack trace error handler when function return
 */

class ErrorPrivate;

class Q_DECL_DEPRECATED Error : private QSharedPointer<ErrorPrivate>
{
public:
    Error() = default;

    explicit Error(const char *file, int line, const char *func, int code, const QString &msg);

    explicit Error(
      const char *file, int line, const char *func, const Error &reason, const QString &msg);

    inline bool operator==(const Error &other) const { return m_code == other.m_code; }

    explicit operator bool() const;

    inline bool operator!() const { return !this->operator bool(); }

    int code() const;

    QString message() const;

    QString toJson() const;

    friend QDebug operator<<(QDebug debug, const linglong::util::Error &result);
    friend void PrintTo(const linglong::util::Error &obj, ::std::ostream *os);
    friend class ErrorPrivate;

private:
    int m_code;
};

QDebug operator<<(QDebug dbg, const Error &result);
void PrintTo(const linglong::util::Error &obj, ::std::ostream *os);

} // namespace util
} // namespace linglong

#define NewError(code, message) \
    linglong::util::Error(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, code, message)

#define WrapError(reason, message)            \
    linglong::util::Error(QT_MESSAGELOG_FILE, \
                          QT_MESSAGELOG_LINE, \
                          QT_MESSAGELOG_FUNC, \
                          reason,             \
                          message)

#define Success() linglong::util::Error()

#endif
