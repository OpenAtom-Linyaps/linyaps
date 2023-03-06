/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_RESULT_H_
#define LINGLONG_SRC_MODULE_UTIL_RESULT_H_

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
          int code = 0,
          const QString &msg = "")
        : errorCode(code)
        , msgMeta(MessageMeta{
                  .file = file,
                  .line = line,
                  .func = func,
                  .message = msg,
          })
        , msgMetaList(base.msgMetaList)
    {
        msgMetaList.push_back(msgMeta);
    };

    Error(const char *file, int line, const char *func, int code = 0, const QString &msg = "")
        : errorCode(code)
        , msgMeta(MessageMeta{
                  .file = file,
                  .line = line,
                  .func = func,
                  .message = msg,
          })
    {
        msgMetaList.push_back(msgMeta);
    };

    Error(const char *file, int line, const char *func, const QString &msg, const Error &base)
        : errorCode(base.code())
        , msgMeta(MessageMeta{
                  .file = file,
                  .line = line,
                  .func = func,
                  .message = msg,
          })
        , msgMetaList(base.msgMetaList)
    {
        msgMetaList.push_back(msgMeta);
    };

    bool success() const { return 0 == errorCode; }

    int code() const { return errorCode; }

    QString message() const { return msgMeta.message; }

    Error &operator<<(const QString &msg)
    {
        msgMeta.message = msg;
        msgMetaList.push_back(msgMeta);
        return *this;
    }

    Error &operator<<(int code)
    {
        errorCode = code;
        return *this;
    }

    QString toJson() const
    {
        QJsonObject obj;
        obj["code"] = errorCode;
        obj["message"] = msgMeta.message;
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

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

inline QDebug operator<<(QDebug dbg, const Error &result)
{
    dbg << "\n";
    for (const auto &meta : result.msgMetaList) {
        dbg << QString(meta.file) + ":" + QString("%1").arg(meta.line) + ":" + QString(meta.func)
            << "\n";
        dbg << meta.message << "\n";
    }
    return dbg;
}

} // namespace util
} // namespace linglong

#define NewError(...) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define NoError() linglong::util::Error(__FILE__, __LINE__, __FUNCTION__)

#define WrapError(base, msg) linglong::util::Error(__FILE__, __LINE__, __FUNCTION__, msg, base)
#endif
