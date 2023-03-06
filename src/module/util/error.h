/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_ERROR_H_
#define LINGLONG_SRC_MODULE_UTIL_ERROR_H_

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <tuple>

namespace linglong {
namespace util {

/*!
 * Error is a stack trace error handler when function return
 */
class Error
{
public:
    Error() = default;

    Error(const char *file,
          int line,
          const char *func,
          const Error &base,
          int code,
          const QString &msg);

    Error(const char *file, int line, const char *func, int code = 0, const QString &msg = "");

    Error(const char *file, int line, const char *func, const QString &msg, const Error &base);

    bool success() const;

    int code() const;

    QString message() const;

    Error &operator<<(const QString &msg);

    Error &operator<<(int code);

    QString toJson() const;

    friend QDebug operator<<(QDebug d, const linglong::util::Error &result);

private:
    struct MessageMeta
    {
        const char *file;
        int line;
        const char *func;
        QString message;
    };

    int errorCode = 0;
    MessageMeta msgMeta;
    QList<MessageMeta> msgMetaList{};
};

QDebug operator<<(QDebug dbg, const Error &result);

} // namespace util
} // namespace linglong

#define NewError(...) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define NoError() linglong::util::Error(__FILE__, __LINE__, __FUNCTION__)

#define WrapError(base, msg) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, msg, base)

#endif
